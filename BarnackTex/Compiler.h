#pragma once

#include <filesystem>

#include "Tokenizer.h"
#include "Elements.h"
#include "here.h"

namespace bartex
	{
	class compiler
		{
		public:
			inline static void compile_project(const std::filesystem::path& root_source, const std::filesystem::path& root_target)
				{
				std::filesystem::remove_all(root_target);
				compile_directory(root_source, root_target / "Content");
				std::filesystem::copy(here() / "HTML", root_target);
				}

		private:
			inline static void compile_directory(const std::filesystem::path& root_source, const std::filesystem::path& root_target)
				{
				std::filesystem::create_directories(root_target);

				for (const auto& entry : std::filesystem::directory_iterator{root_source})
					{
					auto path_source{entry.path()};

					if (!path_source.has_filename()) { continue; }

					auto path_target{root_target / path_source.filename()};

					if (std::filesystem::is_regular_file(path_source))
						{
						compile_file(path_source, path_target);
						}
					else if (std::filesystem::is_directory(path_source))
						{
						compile_directory(path_source, path_target);
						}
					}
				}

			inline static void compile_file(const std::filesystem::path& source, const std::filesystem::path& target)
				{
				if (source.has_extension() && source.extension() == ".bartex")
					{
					compile_file_bartex(source, target);
					}
				else { compile_file_other(source, target); }
				}

			inline static void compile_file_bartex(const std::filesystem::path& source, std::filesystem::path target)
				{
				try
					{
					target.replace_extension(".html");

					bartex::Reader reader{source};
					bartex::Tokenizer tokenizer{reader};

					bartex::Document doc{tokenizer};

					std::ofstream os(target);
					doc.to_HTML(os);
					}
				catch (std::runtime_error& e) 
					{
					std::cout << "Failed compilation of file: " << target.string() << "\n";
					std::cout << e.what() << std::endl; 
					}
				//catch (std::logic_error e) {};
				};
			inline static void compile_file_other(const std::filesystem::path& source, const std::filesystem::path& target)
				{
				std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing);
				}

		};

	}