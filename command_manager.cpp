#include "command_manager.hpp"
#include "input.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cassert>
#include <unordered_set>

//TODO update to so uses assert at more places & rethink auto-fill, cause it still has some problems (false input on enter)

namespace cmd {

	void CommandManager::message(const std::string& message) {
		if (Debug_mode)
			std::cout << '[' + lib_id + "] " << message << std::endl;
	}

	static ContextId globalContext() {
		static constexpr char key = 0;
		return &key;
	}

	CommandManager::CommandManager() {
		registerCommand(globalContext(), "help", "List available commands",
			[this](const std::vector<std::string>& args, const ContextList& contexts) {
				bool showAliases = false;
				if (!args.empty()) {
					std::string arg = args[0];
					std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
					showAliases = (arg == "aliases");
				}
				printHelp(contexts, showAliases);
			});
	}

	CommandManager& CommandManager::instance() {
		static CommandManager mgr;
		return mgr;
	}

	CommandManager::Command* CommandManager::findInList(CommandList& list, const std::string& name) const {
		for (auto& pair : list)
			if (pair.first == name) return pair.second.get();
		return nullptr;
	}

	const CommandManager::Command* CommandManager::registerCommand(ContextId ctx, const std::string& name, const std::string& description,
		std::function<void(const std::vector<std::string>&, const ContextList&)> handler)
	{
		auto& ctxMap = commands_[ctx];
		if (findInList(ctxMap, name)) {
			message("Duplicated '" + name + "' registry entry!");
			return nullptr;
		}

		auto cmd = std::make_unique<Command>();
		cmd->name = name;
		cmd->description = description;
		cmd->handler = std::move(handler);

		const Command* ptr = cmd.get();
		ctxMap.emplace_back(name, std::move(cmd));
		return ptr;
	}

bool CommandManager::registerAlias(ContextId ctx, const std::string& alias, const std::string& target) {
    auto& ctxList = commands_[ctx];

    Command* targetCmd = findInList(ctxList, target);
    if (!targetCmd || targetCmd->is_alias) {
        message("Target '" + target + "' for '" + alias + "' not found!");
        return false;
    }

    if (findInList(ctxList, alias)) {
        message("For target '" + target + "' duplicated alias '" + alias + "' has been found!");
        return false;
    }

    auto cmd = std::make_unique<Command>();
    cmd->name = alias;
    cmd->description = "";
    cmd->handler = targetCmd->handler;
    cmd->is_alias = true;
    cmd->alias_target = target;

    ctxList.emplace_back(alias, std::move(cmd));
    return true;
}

	bool CommandManager::parseInput(const std::string& input, std::string& cmd, std::vector<std::string>& args) const {
		std::istringstream iss(input);
		if (!(iss >> cmd)) return false;

		std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
		std::string token;
		while (iss >> token) args.push_back(token);

		return true;
	}

	const CommandManager::Command* CommandManager::execute(const ContextList& contexts, const std::string& input, ConflictPolicy policy) {
		std::string cmdName;
		std::vector<std::string> args;
		if (!parseInput(input, cmdName, args)) return nullptr;

		ContextList searchOrder = contexts;
		searchOrder.push_back(globalContext());

		if (policy == ConflictPolicy::Error) {
			std::unordered_map<std::string, ContextId> seen;
			for (ContextId ctx : searchOrder) {
				auto ctxIt = commands_.find(ctx);
				if (ctxIt == commands_.end()) continue;
				for (const auto& pair : ctxIt->second) {
					if (pair.second->is_alias) continue;
					auto it = seen.find(pair.first);
					if (it != seen.end() && it->second != ctx)
						assert(false && "Command name conflict across contexts");
					seen[pair.first] = ctx;
				}
			}
		}

		for (ContextId ctx : searchOrder) {
			auto ctxIt = commands_.find(ctx);
			if (ctxIt == commands_.end()) continue;
			Command* cmd = findInList(ctxIt->second, cmdName);
			if (!cmd) continue;
			if (cmd->handler)
				cmd->handler(args, contexts);
			return cmd;
		}

		std::cout << "Unknown command: " << cmdName << "\n";
		return nullptr;
	}

	void CommandManager::runInteractive(const ContextList& contexts, bool modal, ConflictPolicy policy) {
		enableRawMode();
		std::string line;
		size_t cursor = 0;
		std::vector<std::string> matches;
		size_t matchIndex = 0;

		std::cout << "> ";
		while (true) {
			KeyPress kp = readKey();
			switch (kp.key) {
			case Key::Enter: {
				std::cout << "\n";
				if (!line.empty()) {
					const Command* executed = execute(contexts, line, policy);
					if (!modal || (executed && executed->name != "help")) {
						disableRawMode();
						return;
					}
				}
				line.clear();
				cursor = 0;
				matches.clear();
				std::cout << "> ";
				break;
			}
			case Key::Backspace: {
				if (cursor > 0) {
					line.erase(cursor - 1, 1);
					cursor--;
					matches.clear();
					redrawLine(line, cursor);
				}
				break;
			}
			case Key::Tab: {
				if (matches.empty())
					matches = getCompletions(contexts, line);
				if (matches.empty()) break;
				line = matches[matchIndex % matches.size()];
				cursor = line.size();
				matchIndex++;
				redrawLine(line, cursor);
				break;
			}
			case Key::ArrowLeft: { if (cursor > 0) { cursor--; redrawLine(line, cursor); } break; }
			case Key::ArrowRight: { if (cursor < line.size()) { cursor++; redrawLine(line, cursor); } break; }
			case Key::Character: {
				line.insert(cursor, 1, kp.ch);
				cursor++;
				matches.clear();
				matchIndex = 0;
				redrawLine(line, cursor);
				break;
			}
			default: break;
			}
		}
		disableRawMode();
	}

	void CommandManager::printHelp(const ContextList& contexts, bool showAliases, ConflictPolicy policy) const {
		std::cout << "Available commands:\n";
		std::unordered_map<std::string, ContextId> seen;
		std::unordered_map<std::string, std::vector<std::string>> aliasMap;

		// Conflict check pass
		for (ContextId ctx : contexts) {
			auto ctxIt = commands_.find(ctx);
			if (ctxIt == commands_.end()) continue;
			for (const auto& pair : ctxIt->second) {
				if (pair.second->is_alias) continue;
				auto it = seen.find(pair.first);
				if (it != seen.end() && it->second != ctx) {
					if (policy == ConflictPolicy::Error)
						assert(false && "Command name conflict across contexts");
				}
				else seen[pair.first] = ctx;
			}
		}

		seen.clear();

		// Print pass
		bool isEmpty = true;
		for (ContextId ctx : contexts) {
			auto ctxIt = commands_.find(ctx);
			if (ctxIt == commands_.end()) continue;
			isEmpty = false;
			if (showAliases) {
				for (const auto& pair : ctxIt->second) {
					if (pair.second->is_alias)
						aliasMap[pair.second->alias_target].push_back(pair.first);
				}
			}
			for (const auto& pair : ctxIt->second) {
				if (pair.second->is_alias) continue;
				if (seen.count(pair.first)) continue;
				seen[pair.first] = ctx;
				std::cout << "  " << pair.first;
				auto it = aliasMap.find(pair.first);
				if (it != aliasMap.end() && !it->second.empty()) {
					std::cout << " (";
					for (size_t i = 0; i < it->second.size(); ++i) {
						if (i > 0) std::cout << ", ";
						std::cout << it->second[i];
					}
					std::cout << ")";
				}
				std::cout << " - " << pair.second->description << "\n";
			}
		}
		if (isEmpty)
			std::cout << "  No command(s) have been registered for the given context(s)." << std::endl;
		else if (!showAliases) {
			std::cout << "Use 'help aliases' to display the command aliases." << std::endl;
		}
	}

	void CommandManager::redrawLine(const std::string& line, size_t cursor) const {
		std::string buf;
		buf += "\033[?25l";                          // hide cursor
		buf += "\r\033[K";                           // clear line
		buf += "> ";
		buf += line;
		buf += "\r\033[";
		buf += std::to_string(cursor + 2);
		buf += "C";
		buf += "\033[?25h";                          // show cursor
		std::cout << buf;
		std::cout.flush();
	}

	std::vector<std::string> CommandManager::getCompletions(const ContextList& contexts, const std::string& prefix) const {
		std::vector<std::string> results;
		ContextList searchOrder = contexts;
		searchOrder.push_back(globalContext());

		std::unordered_set<std::string> seen;
		for (ContextId ctx : searchOrder) {
			auto ctxIt = commands_.find(ctx);
			if (ctxIt == commands_.end()) continue;
			for (const auto& pair : ctxIt->second) {
				if (pair.second->is_alias) continue;
				if (seen.count(pair.first)) continue;
				if (pair.first.substr(0, prefix.size()) == prefix) {
					results.push_back(pair.first);
					seen.insert(pair.first);
				}
			}
		}
		return results;
	}
}
