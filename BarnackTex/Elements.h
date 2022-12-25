#pragma once

#include <string>
#include <ostream>

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>

#include "Tokenizer.h"

namespace bartex
	{
	//////////////////////////////////////////////////////////////////////////////////////// Base class

	struct Element
		{
		virtual std::ostream& to_HTML(std::ostream& os) const noexcept = 0;
		};

	using eptr = std::unique_ptr<Element>;
	eptr parse_command(Tokenizer& tokenizer, bool stop_as_lines);

	//////////////////////////////////////////////////////////////////////////////////////// General Containers

	struct Wrapper
		{
		eptr element;
		std::ostream& to_HTML(std::ostream& os) const noexcept { return element->to_HTML(os); }
		};

	struct Group
		{
		std::vector<eptr> elements;
		std::ostream& to_HTML(std::ostream& os) const noexcept { for (const auto& element : elements) { element->to_HTML(os); } return os; }
		std::ostream& to_HTML(std::ostream& os, const std::string& pre, const std::string& post) const noexcept { for (const auto& element : elements) { os << pre; element->to_HTML(os); os << post; } return os; }
		};

	//////////////////////////////////////////////////////////////////////////////////////// Tree leafs

	struct Word : Element
		{
		Word(Token<std::string> tok) : tok{tok} {}

		Token<std::string> tok;
		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override final { return os << tok.value; }
		};

	//////////////////////////////////////////////////////////////////////////////////////// Container readers

	struct Sequence : Group, Element
		{
		Sequence(Tokenizer& tokenizer)
			{
			tokenizer.skip_whitespace();
			if (!tokenizer.has_next()) { throw parser_error{"Expected new sequence."}; }

			while (tokenizer.has_next())
				{
				Token<std::string> tok = tokenizer.next();

				if (tok.value == "}") { tokenizer.back(tok); return; }
				else if (tok.value == "{") { throw parser_error{"Unexpected \"{\".", tok.line, tok.pos}; }
				else if (tok.value == "\n")
					{
					if (!tokenizer.has_next()) { return; } //if reached end of file after single newline, end sequence
					Token<std::string> second = tokenizer.next();

					if (second.value == "\n")
						{  //if double newline, end sequence; pushing two newlines to end potential sequences which might be wrapping this
						tokenizer.back(tok);
						tokenizer.back(second);
						return;
						}
					else //if there was a single newline, replace with a space, put the token after that newline back and proceed with the loop.
						{
						tok.value = " ";
						elements.push_back(std::move(std::make_unique<Word>(tok)));
						tokenizer.back(second);
						}
					}
				else if (tok.value == "\\") { elements.push_back(parse_command(tokenizer, false)); }
				else { elements.push_back(std::move(std::make_unique<Word>(tok))); }
				}
			//if reached end of file, end sequence
			}

		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override { return Group::to_HTML(os); }
		};

	struct Line : Group, Element
		{
		Line(Tokenizer& tokenizer)
			{
			tokenizer.skip_whitespace(false);

			if (!tokenizer.has_next()) { throw parser_error{"Expected new sequence."}; }

			while (tokenizer.has_next())
				{
				Token<std::string> tok = tokenizer.next();

				if (tok.value == "}") { tokenizer.back(tok); return; }
				else if (tok.value == "{") { throw parser_error{"Unexpected \"{\".", tok.line, tok.pos}; }
				else if (tok.value == "\n") { tokenizer.back(tok); return; }
				else if (tok.value == "\\") { elements.push_back(parse_command(tokenizer, true)); }
				else { elements.push_back(std::move(std::make_unique<Word>(tok))); }
				}
			//if reached end of file, end sequence
			std::cout << "LINE END" << std::endl;
			}

		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override { return Group::to_HTML(os); }
		};

	struct Sequence_lines : Group, Element
		{
		Sequence_lines(Tokenizer& tokenizer);

		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override { return Group::to_HTML(os); }
		};

	struct Block : Group, Element
		{
		Block(Tokenizer& tokenizer)
			{
			tokenizer.skip_whitespace();

			if (!tokenizer.has_next()) { throw parser_error{"Expected \"{\"."}; }
			Token tok = tokenizer.next();
			if (tok.value != "{") { throw parser_error{"Expected Block; no \"{\" found.", tok.line, tok.pos}; }

			while (tokenizer.has_next())
				{
				Token tok = tokenizer.next();
				if (tok.value == "}") { return; }
				else if (tok.value == "\\") { elements.push_back(parse_command(tokenizer, false)); }
				else
					{
					tokenizer.back(tok);
					elements.push_back(std::move(std::make_unique<Sequence>(tokenizer)));
					}
				}

			throw parser_error{"End of File reached but a block has not been closed. Expected \"}\"."};
			}

		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override { return Group::to_HTML(os); }
		};

	//////////////////////////////////////////////////////////////////////////////////////// Other

	struct Document : Group, Element
		{
		Document(Tokenizer& tokenizer)
			{
			tokenizer.skip_whitespace();

			while (tokenizer.has_next())
				{
				Token<std::string> tok = tokenizer.next();

				if (tok.value == "}") { throw parser_error{"Unexpected \"}\", no block to be closed.", tok.line, tok.pos}; }
				else if (tok.value == "{") { throw parser_error{"Unexpected \"{\".", tok.line, tok.pos}; }
				else if (tok.value == "\n") {}
				else if (tok.value == "\\") { elements.push_back(parse_command(tokenizer, false)); }
				else
					{//If not command simulate paragraph
					tokenizer.back(tok);
					tokenizer.back({"paragraph",  tok.line, tok.pos});
					tokenizer.back({"\\", tok.line, tok.pos});
					}

				tokenizer.skip_whitespace();
				}
			//if reached end of file, end sequence
			}


		virtual std::ostream& to_HTML(std::ostream& os) const noexcept final override
			{
			Group::to_HTML(os, "", "\n");
			return os;
			}
		};

	//////////////////////////////////////////////////////////////////////////////////////// Commands

	struct Command : Element
		{
		Command(const Token<std::string>& command) : command_tok{command} {}
		Token<std::string> command_tok;
		};

	template <utils::Template_string html_tag>
	struct Command_html_tag : Command, Wrapper
		{
		Command_html_tag(const Token<std::string>& command_tok, Tokenizer& tokenizer, bool stop_as_lines) : Command{command_tok}
			{
			tokenizer.skip_whitespace(!stop_as_lines);

			if (!tokenizer.has_next()) { throw parser_error{"Command \"" + command_tok.value + "\" expets a paragraph or a block.", command_tok.line, command_tok.pos}; }
			Token<std::string> tok = tokenizer.next();
			tokenizer.back(tok);


			if (tok.value == "{") { element = std::make_unique<Block>(tokenizer); }
			else if (stop_as_lines) { element = std::make_unique<Line>(tokenizer); }
			else { element = std::make_unique<Sequence>(tokenizer); }
			}

		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override
			{
			os << " <" << html_tag.value << "> ";
			Wrapper::to_HTML(os);
			return os << " </" << html_tag.value << "> ";
			}
		};

	using cptr = std::unique_ptr<Command>;

	// ====================================================================================================================================================== COMMANDS DEFINITIONS BEG

	struct Command_incl : Command_html_tag<"include">
		{
		Command_incl(const Token<std::string>& command_tok, Tokenizer& tokenizer, bool stop_as_lines) : Command_html_tag{command_tok, tokenizer, stop_as_lines}
			{
			std::stringstream ss;
			Wrapper::to_HTML(ss);
			}
		};
	struct Command_ref : Command_html_tag<"refer">
		{
		Command_ref(const Token<std::string>& command_tok, Tokenizer& tokenizer, bool stop_as_lines) : Command_html_tag{command_tok, tokenizer, stop_as_lines}
			{
			std::stringstream ss;
			Wrapper::to_HTML(ss);
			}
		};

	struct Command_line : Command, Wrapper
		{
		Command_line(const Token<std::string>& command_tok, Tokenizer& tokenizer, bool stop_as_lines) : Command{command_tok}
			{
			tokenizer.skip_whitespace(false);
			if (!tokenizer.has_next()) { throw parser_error{"Command \"" + command_tok.value + "\" expets a sequence of \"\\line\" commands or a block.", command_tok.line, command_tok.pos}; }

			Token<std::string> tok = tokenizer.next();
			tokenizer.back(tok);

			if (tok.value == "{") { element = std::make_unique<Block>(tokenizer); }
			else { element = std::make_unique<Line>(tokenizer); }
			}
		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override { Wrapper::to_HTML(os); return os; }

		};

	struct Command_code : Command, Wrapper
		{
		Command_code(const Token<std::string>& command_tok, Tokenizer& tokenizer, bool stop_as_lines) : Command{command_tok}
			{
			auto _error = parser_error{"Command \"" + command_tok.value + "\" expets a language token (c, cpp, java, js, ...) and a block containing the filename.", command_tok.line, command_tok.pos};

			tokenizer.skip_whitespace(false);
			if (!tokenizer.has_next()) { throw _error; }

			Token<std::string> tok = tokenizer.next();

			if (tok.value == "{") { tokenizer.back(tok); element = std::make_unique<Block>(tokenizer); return; }
			else { language = tok; }

			tokenizer.skip_whitespace(false);
			if (!tokenizer.has_next()) { throw _error; }

			tok = tokenizer.next();
			tokenizer.back(tok);

			if (tok.value == "{") { element = std::make_unique<Block>(tokenizer); }
			else { throw _error; }
			}

		Token<std::string> language;

		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override 
			{
			os << " <code";
			if (language.value != "") { os << " language=\"" + language.value + "\""; }
			os << "> ";
			Wrapper::to_HTML(os);
			return os << "</code> ";
			}

		};

	struct Command_list : Command
		{
		Command_list(const Token<std::string>& command_tok, Tokenizer& tokenizer, bool stop_as_lines) : Command{command_tok}
			{
			tokenizer.skip_whitespace(!stop_as_lines);

			if (!tokenizer.has_next()) { throw parser_error{"Command \"" + command_tok.value + "\" expets a sequence of \"\\line\" commands or a block.", command_tok.line, command_tok.pos}; }

			Token<std::string> tok = tokenizer.next();
			tokenizer.back(tok);

			if (tok.value == "{") { group = std::make_unique<Block>(tokenizer); }
			else if (stop_as_lines) { throw parser_error{"Cannot use a list command inside a command that expects a line, except by opening a \"{...}\" block in the same line.", tok.line, tok.pos}; }
			else { group = std::make_unique<Sequence_lines>(tokenizer); }
			}

		virtual std::ostream& to_HTML(std::ostream& os) const noexcept override
			{
			os << " <ul> \n";
			for (const auto& element : group->elements)
				{
				os << " <li> ";
				element->to_HTML(os);
				os << " </li> \n";
				}
			return os << " </ul> ";
			}

		std::unique_ptr<Group> group;
		};

	// ====================================================================================================================================================== COMMANDS DEFINITIONS END
	inline std::unordered_map<std::string, std::function<cptr(const Token<std::string>&, Tokenizer&, bool)>> commands_map
		{
		//Simple HTML tags
			// Defult tags
			{"title",     [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_html_tag<"h1">>(command, tokenizer, stop_as_lines); }},
			{"paragraph", [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_html_tag<"p" >>(command, tokenizer, stop_as_lines); }},
			{"bold",      [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_html_tag<"b" >>(command, tokenizer, stop_as_lines); }},
			{"italic",    [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_html_tag<"i" >>(command, tokenizer, stop_as_lines); }},

			// Custom tags

		// Complex tags
			// Default tags
			{"list",      [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_list>(command, tokenizer, stop_as_lines); }},

			// Custom  tags
			{"code",      [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_code>(command, tokenizer, stop_as_lines); }},
			{"include",   [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_incl>(command, tokenizer, stop_as_lines); }},
			{"refer",     [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_incl>(command, tokenizer, stop_as_lines); }},

		// Custom commands
			{"line",      [] (const Token<std::string>& command, Tokenizer& tokenizer, bool stop_as_lines) -> cptr { return std::make_unique<Command_line>(command, tokenizer, stop_as_lines); }},
		};

	inline eptr parse_command(Tokenizer& tokenizer, bool stop_as_lines)
		{
		if (!tokenizer.has_next()) { throw parser_error{"Expected command."}; }
		Token tok = tokenizer.next();

		auto it = commands_map.find(tok.value);
		if (it == commands_map.end()) { throw parser_error{"Unknown command \"" + tok.value + "\".", tok.line, tok.pos}; }

		return it->second(tok, tokenizer, stop_as_lines);
		}










	inline Sequence_lines::Sequence_lines(Tokenizer& tokenizer)
		{
		tokenizer.skip_whitespace();
		if (!tokenizer.has_next()) { throw parser_error{"Expected sequence of lines."}; }

		while (tokenizer.has_next())
			{
			tokenizer.skip_whitespace(false);

			Token<std::string> tok = tokenizer.next();

			if (tok.value == "}") { tokenizer.back(tok); return; }
			else if (tok.value == "\n")
				{
				if (!tokenizer.has_next()) { return; } //if reached end of file after single newline, end sequence
				Token<std::string> second = tokenizer.next();
				if (second.value == "\n")
					{  //if double newline, end sequence; pushing two newlines to end potential sequences which might be wrapping this
					tokenizer.back(tok);
					tokenizer.back(second);
					return;
					}
				else //Second was first token of next line
					{
					tokenizer.back(second);
					}
				}
			else if (tok.value == "\\")
				{
				if (!tokenizer.has_next()) { throw parser_error{"Expected command \"\\line\", or no command (will be \"\\line\" implicitly) in this sequence.", tok.line, tok.pos}; }
				Token<std::string> second = tokenizer.next();
				if (second.value != "line") //If there was a non-line command, push back \line then that command
					{
					tokenizer.back(second);
					tokenizer.back(tok);
					tokenizer.back({"line", tok.line, tok.pos});
					tokenizer.back({"\\", tok.line, tok.pos});
					}
				else { elements.push_back(std::make_unique<Command_line>(second, tokenizer, false)); }
				}
			else //includes if (tok.value == "{")
				{
				tokenizer.back(tok);
				tokenizer.back({"line", tok.line, tok.pos});
				tokenizer.back({"\\", tok.line, tok.pos});
				}
			}
		//if reached end of file, end sequence
		}
	}