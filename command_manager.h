#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

/***
* Command Manager singleton class
* Remake this crap
***/

namespace cmd {

	using ContextId = const void*;

	struct Command {
		std::string name;
		std::string description;
		std::function<void(const std::vector<std::string>&)> handler;
		bool is_alias = false;
		std::string alias_target;
	};

	class CommandManager {
		std::string lib_name = "CMD_MNGR";

		bool Debug_mode = false;

		CommandManager(){}
		CommandManager(const CommandManager&) = delete;
		CommandManager& operator=(const CommandManager&) = delete;

		bool parseInput(const std::string& input, std::string& cmd, std::vector<std::string>& args) const;

		void message(const std::string& message);
		/***
		* Internal storage:
		* context : (name : command)
		***/
		std::unordered_map<ContextId,
			std::unordered_map<std::string, std::unique_ptr<Command>>
		> commands_;

	public:
		static CommandManager&	instance();
				const Command*	registerCommand	(ContextId ctx, const std::string& name, const std::string& description,
												std::function<void(const std::vector<std::string>&)> handler);
						bool	registerAlias	(ContextId ctx, const std::string& alias, const std::string& target);
				const Command*	execute			(ContextId ctx, const std::string& input);
						void	runInteractive	(ContextId ctx, bool modal = false);
						void	printHelp		(ContextId ctx, bool showAliases = true) const;
	};
}

#endif