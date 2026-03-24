#include "command_manager.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace cmd {

	void CommandManager::message(const std::string& message) {
		if (Debug_mode)
			std::cout << '[' + lib_name + "] " << message << std::endl;
	}
	
	/***
	Creates and manages a singleton instance
	***/
	CommandManager& CommandManager::instance() {
		static CommandManager mgr;
		return mgr;
	}


	/***
	Register an alias in the command table.
	INFO: Overloading isn't supported.
	***/
	const Command* CommandManager::registerCommand(ContextId ctx, const std::string& name, const std::string& description,
		std::function<void(const std::vector<std::string>&)> handler)
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


	/***
	Register an alias in the command table to a command.
	INFO: Overloading isn't supported.
	WARNING: Can't be called on an another alias
	***/
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


	/***
	Private praser function
	***/
	bool CommandManager::parseInput(const std::string& input, std::string& cmd, std::vector<std::string>& args) const {
		std::istringstream iss(input);
		if (!(iss >> cmd)) return false;

		std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
		std::string token;
		while (iss >> token) args.push_back(token);

		return true;
	}


	/***
	Prase, searches, and executes the command from the command list given trough the input (has argument support).
	***/
	const Command* CommandManager::execute(ContextId ctx, const std::string& input) {
		std::string cmdName;
		std::vector<std::string> args;
		if (!parseInput(input, cmdName, args)) return nullptr;

		if (cmdName == "help") {
			if (!args.empty())
				std::transform(args[0].begin(), args[0].end(), args[0].begin(), ::tolower);
			printHelp(ctx, !args.empty() && args[0] == "aliases");
			return nullptr;
		}

		auto ctxIt = commands_.find(ctx);
		auto cmdIt = ctxIt->second.find(cmdName);

		if (ctxIt == commands_.end() || cmdIt == ctxIt->second.end())
		{
			std::cout << "Unknown command: " << cmdName << "\n";
			return nullptr;
		}

		if (cmdIt->second->handler)
			cmdIt->second->handler(args);
			return cmdIt->second.get();


	}


	/***
	Interrupts the program running for an internal command loop.
	***/
	void CommandManager::runInteractive(ContextId ctx, bool modal) {
		std::string line;
		while (true) {
			std::cout << "> ";
			if (!std::getline(std::cin, line)) break;
			const Command* executed = execute(ctx, line);
			if (!modal && executed && executed->name != "help") break;
		}
	}


	/***
	Context aware command list printing function, with alias support if enabled.
	***/
	void CommandManager::printHelp(ContextId ctx, bool showAliases) const {

		std::cout << "Available commands:\n";

		auto ctxIt = commands_.find(ctx);
		if (ctxIt == commands_.end()) return;

		std::unordered_map<std::string, std::vector<std::string>> aliasMap;
		if (showAliases) {
			for (const auto& pair : ctxIt->second) {
				if (pair.second->is_alias)
					aliasMap[pair.second->alias_target].push_back(pair.first);
			}
		}

		for (const auto& pair : ctxIt->second) {
			if (pair.second->is_alias) continue;
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
}
