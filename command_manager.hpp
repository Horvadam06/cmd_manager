#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

/***
* Command Manager singleton class
* Created by.: Horváth Ádám József, early 2026.
***/

namespace cmd {

	using ContextId = const void*;
	using ContextList = std::vector<ContextId>;

	enum class ConflictPolicy {
		Error,      // default
		FirstWins,  // for explicit user over-write
	};

	class CommandManager {
		/***
		Info thingy
		***/
		std::string lib_id = "CMD_MNGR";
		bool Debug_mode = false;

		/***
		Command registry parameters
		***/
		struct Command {
			std::string name;
			std::string description;
			std::function<void(const std::vector<std::string>&, const ContextList&)> handler;
			bool is_alias = false;
			std::string alias_target;
		};

		/***
		Registers global & contextless command(s)
		***/
		CommandManager();
		CommandManager(const CommandManager&) = delete;
		CommandManager& operator=(const CommandManager&) = delete;

		/***
		Private praser function
		***/
		bool parseInput(const std::string& input, std::string& cmd, std::vector<std::string>& args) const;

		/***
		For system output, using a library ID
		***/
		void message(const std::string& message);

		/***
		Internal storage:
		context : (name : command)
		***/
		using CommandList = std::vector<std::pair<std::string, std::unique_ptr<Command>>>;
		std::unordered_map<ContextId, CommandList> commands_;

		void redrawLine(const std::string& line, size_t cursor) const;
		std::vector<std::string> getCompletions(const ContextList& contexts, const std::string& prefix) const;
		Command* findInList(CommandList& list, const std::string& name) const;

	public:
								/***
								Creates & manages a singleton instance
								***/
		static CommandManager&	instance();

								/***
								Register an alias in the command table.
								Register overloading isn't supported.
								@return A ptr to the registered command, null if failed
								***/
				const Command*	registerCommand(ContextId ctx, const std::string& name, const std::string& description,
								std::function<void(const std::vector<std::string>&, const ContextList&)> handler);
								
								/***
								Register an alias in the command table to a command.
								INFO: Overloading isn't supported.
								WARNING: Can't be called on an another alias
								***/
						bool	registerAlias(ContextId ctx, const std::string& alias, const std::string& target);

								/***
								Prase, searches, and executes the command from the command list given trough the input (has argument support).
								***/
				const Command*	execute(const ContextList& contexts, const std::string& input,
								ConflictPolicy policy = ConflictPolicy::Error);

								/***
								Interrupts the program running till a valid (not help) command.
								***/
						void	runInteractive(const ContextList& contexts, bool modal = false,
								ConflictPolicy policy = ConflictPolicy::Error);

								/***
								Context aware command list printing function, with alias support if enabled.
								***/
						void	printHelp(const ContextList& contexts, bool showAliases = false,
								ConflictPolicy policy = ConflictPolicy::Error) const;
	};
}

#endif