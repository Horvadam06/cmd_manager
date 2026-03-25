#include "command_manager.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cassert>

//TODO update to so uses assert at more places

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

	const CommandManager::Command* CommandManager::registerCommand(ContextId ctx, const std::string& name, const std::string& description,
		std::function<void(const std::vector<std::string>&, const ContextList&)> handler)
	{
		auto& ctxMap = commands_[ctx];
		if (ctxMap.find(name) != ctxMap.end()) {
			message("Duplicated '" + name + "' registry entry!");
			return nullptr;
		}

		auto cmd = std::make_unique<Command>();
		cmd->name = name;
		cmd->description = description;
		cmd->handler = std::move(handler);

		const Command* ptr = cmd.get();
		ctxMap[name] = std::move(cmd);
		return ptr;
	}

	bool CommandManager::registerAlias(ContextId ctx, const std::string& alias, const std::string& target) {
		auto& ctxMap = commands_[ctx];

		auto targetIt = ctxMap.find(target);
		if (targetIt == ctxMap.end() || targetIt->second->is_alias) {
			message("Target '" + target + "' for '" + alias + "' not found'!");
			return false;
		}

		if (ctxMap.count(alias)) {
			message("For target '" + target + "' duplicated alias '" + alias + "' has been found'!");
			return false;
		}

		auto cmd = std::make_unique<Command>();
		cmd->name = alias;
		cmd->description = "";
		cmd->handler = targetIt->second->handler;
		cmd->is_alias = true;
		cmd->alias_target = target;

		ctxMap[alias] = std::move(cmd);
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

		// Build full search order: provided contexts + global always last
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
			auto cmdIt = ctxIt->second.find(cmdName);
			if (cmdIt == ctxIt->second.end()) continue;
			if (cmdIt->second->handler)
				cmdIt->second->handler(args, contexts);
			return cmdIt->second.get();
		}

		std::cout << "Unknown command: " << cmdName << "\n";
		return nullptr;
	}

	void CommandManager::runInteractive(const ContextList& contexts, bool modal, ConflictPolicy policy) {
		std::string line;
		while (true) {
			std::cout << "> ";
			if (!std::getline(std::cin, line)) break;
			const Command* executed = execute(contexts, line);
			if (!modal || (executed && executed->name != "help")) break;
		}
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
}
