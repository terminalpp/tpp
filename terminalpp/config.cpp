#include <fstream>

#include "helpers/filesystem.h"

#include "application.h"
#include "config.h"



namespace tpp { 

    Config & Config::Setup(int argc, char * argv[]) {
        MARK_AS_UNUSED(argc);
        MARK_AS_UNUSED(argv);
        Config & config = Instance();
        bool saveSettings = false;
        std::string filename = GetSettingsFile();
        {
            std::ifstream f{filename};
            if (f.good()) {
				try {
					try {
						JSON settings{JSON::Parse(f)};
						VerifyConfigurationVersion(settings);
						// specify, check errors, make copy if wrong
						saveSettings = config.update(settings, [& saveSettings, & filename](JSONError && e){
							Application::Instance()->alert(STR(e.what() << " while parsing terminalpp settings at " << filename));
							saveSettings = true;
						});
					} catch (JSONError & e) {
						e.setMessage(STR(e.what() << " while parsing terminalpp settings at " << filename));
						throw;
					}
				} catch (std::exception const & e) {
					Application::Instance()->alert(e.what());
					saveSettings = true;
				}
				// if there were any errors with the settings, create backup of the old settings as new settings will be stored. 
				if (saveSettings) {
					std::string backup{MakeUnique(filename)};
					Application::Instance()->alert(STR("New settings file will be saved, backup stored in " << backup));
					f.close();
					Rename(filename, backup);
				}
            } else {
                saveSettings = true;
				Application::Instance()->alert(STR("No settings file found, default settings will be calculated and stored in " << filename));
                // update with empty object, which means that all default values will be calculated
                config.update(JSON::Object());
            }
        }
        // if the settings should be saved, save them now 
        if (saveSettings) {
            CreatePath(GetSettingsFolder());
            std::ofstream f(filename);
            if (!f)
                THROW(IOError()) << "Unable to write to the settings file " << filename;
            // get the saved JSON and store it in the settings file
            f << config.toJSON();
        }
        // parse command line arguments and update the configuration accordingly
        config.parseCommandLine(argc, argv);
        return config;
    }

	std::string Config::GetSettingsFolder() {
        return JoinPath(LocalSettingsFolder(), "terminalpp");
	}

	std::string Config::GetSettingsFile() {
		return JoinPath(GetSettingsFolder(), "settings.json");
	}

	JSON Config::TerminalVersion() {
		return JSON{PROJECT_VERSION};
	}

	void Config::VerifyConfigurationVersion(JSON & userConfig) {
        try {
            if (userConfig["version"]["version"].toString() == PROJECT_VERSION)
                return;
            userConfig["version"].erase("version");
        } catch (...) {
            // if there was an error parsing version, erase entire version
            userConfig.erase("version");
        }
		Application::Instance()->alert(STR("Settings version differs from current terminal version (" << PROJECT_VERSION << "). The configuration will be updated to the new version."));
	}

	JSON Config::DefaultTelemetryDir() {
		return JSON{JoinPath(JoinPath(TempDir(), "terminalpp"),"telemetry")};
	}

	JSON Config::DefaultRemoteFilesDir() {
		return JSON{JoinPath(JoinPath(TempDir(), "terminalpp"),"remoteFiles")};
	}

	JSON Config::DefaultFontFamily() {
#if (defined ARCH_WINDOWS)
		return JSON{"Consolas"};
#elif (defined ARCH_MACOS)
        return JSON{"Courier New"};
#else
		char const * fonts[] = { "Monospace", "DejaVu Sans Mono", "Nimbus Mono", "Liberation Mono", nullptr };
		char const ** f = fonts;
		while (*f != nullptr) {
			std::string found{Exec(Command("fc-list", { *f }))};
			if (! found.empty())
			    return JSON{*f};
			++f;
		}
		Application::Instance()->alert("Cannot guess valid font - please specify manually for best results");
		return JSON{""};
#endif
	}

	JSON Config::DefaultDoubleWidthFontFamily() {
		return DefaultFontFamily();
	}

    JSON Config::DefaultSessions() {
        JSON result{JSON::Array()};
        std::string defaultSessionName = "default_shell";
    #if (defined ARCH_MACOS)
        defaultSessionName = "Default login shell";
        JSON shell{JSON::Object()};
        shell.setComment("Default login shell");
        shell.add("name", JSON{defaultSessionName});
        shell.add("command", JSON::Parse(STR("[\"" << getpwuid(getuid())->pw_shell << "\", \"--login\"]")));
        shell.add("workingDirectory", JSON{HomeDir()});
        result.add(shell);        
    #elif (defined ARCH_UNIX)
        JSON shell{JSON::Object()};
        shell.setComment("Default login shell");
        shell.add("name", JSON{defaultSessionName});
        shell.add("command", JSON::Parse(STR("[\"" << getpwuid(getuid())->pw_shell << "\"]")));
        shell.add("workingDirectory", JSON{HomeDir()});
        result.add(shell);        
    #elif (defined ARCH_WINDOWS)
        Win32AddCmdExe(result, defaultSessionName);
        Win32AddPowershell(result, defaultSessionName);
        Win32AddWSL(result, defaultSessionName);
        Win32AddMsys2(result, defaultSessionName);
    #endif
        // update the default session name
        JSON ds{defaultSessionName};
        ds.setComment(Instance().defaultSession.description());
        Instance().defaultSession.set(ds);
        return result;
    }

    #if (defined ARCH_WINDOWS)

    bool Config::WSLIsBypassPresent(std::string const & distro) {
        ExitCode ec; // to silence errors
		std::string output{Exec(Command("wsl.exe", {"--distribution", distro, "--", BYPASS_PATH, "--version"}), &ec)};
		return output.find("ConPTY bypass for terminal++, version") == 0;
    }

    bool Config::WSLInstallBypass(std::string const & distro) {
        try {
            std::string url = STR("https://github.com/terminalpp/terminalpp/releases/latest/download/tpp-bypass-" << distro);
            // disambiguate the ubuntu distribution if not part of the distro name
            if (distro == "Ubuntu") {
                ExitCode ec;
                std::vector<std::string> lines = SplitAndTrim(
                    Exec(Command("wsl.exe", {"--distribution", distro, "--", "lsb_release", "-a"}), &ec),
                    "\n"
                );
                for (std::string const & line : lines) {
                    if (StartsWith(line, "Release:")) {
                        std::string ver = Trim(line.substr(9));
                        url = url + "-" + ver;
                        break;
                    }
                }
            }
            Exec(Command("wsl.exe", {"--distribution", distro, "--", "mkdir", "-p", BYPASS_FOLDER}));
            Exec(Command("wsl.exe", {"--distribution", distro, "--", "wget", "-O", BYPASS_PATH, url}));
            Exec(Command("wsl.exe", {"--distribution", distro, "--", "chmod", "+x", BYPASS_PATH}));
            // just double check
            return WSLIsBypassPresent(distro);
        } catch (...) {
            return false;
        }
    }

    void Config::Win32AddCmdExe(JSON & sessions, std::string & defaultSessionName) {
        defaultSessionName = "cmd.exe";
        JSON session{JSON::Kind::Object};
        session.setComment("cmd.exe");
        session.add("name", JSON{defaultSessionName});
        session.add("command", JSON::Parse(STR("[\"cmd.exe\"]")));
        session.add("workingDirectory", JSON{HomeDir()});
        sessions.add(session);        
    }

    void Config::Win32AddPowershell(JSON & sessions, std::string & defaultSessionName) {
        defaultSessionName = "powershell";
        JSON session{JSON::Kind::Object};
        session.setComment("Powershell - with the default blue background and white text");
        session.add("name", JSON{defaultSessionName});
        session.add("command", JSON::Parse(STR("[\"powershell.exe\"]")));
        session.add("palette", JSON::Parse("{\"defaultForeground\" : \"ffffff\", \"defaultBackground\" : \"#0000ff\" }"));
        session.add("workingDirectory", JSON{HomeDir()});
        sessions.add(session);        
    }

    void Config::Win32AddWSL(JSON & sessions, std::string & defaultSessionName) {
        try {
            std::string defaultSession = defaultSessionName;
            ExitCode ec; // to silence errors
            std::vector<std::string> lines = SplitAndTrim(Exec(Command("wsl.exe", {"--list"}), &ec), "\n");
            // check if we have found WSL
            if (lines.size() < 1 || lines[0] != "Windows Subsystem for Linux Distributions:")
                return;
            // get the installed WSL distributions and determine the default one
            std::vector<std::string> distributions;
            //size_t defaultIndex = 0;
            for (size_t i = 1, e = lines.size(); i != e; ++i) {
                if (EndsWith(lines[i], "(Default)")) {
                    std::string sessionName = Split(lines[i], " ")[0];
                    // skip docker distributions
                    if (sessionName == "docker-desktop" || sessionName == "docker-desktop-data")
                        continue;
                    defaultSession = sessionName;
                    distributions.push_back(sessionName);
                } else {
                    distributions.push_back(lines[i]);
                }
            }
            // if we found no distributions, return
            if (distributions.empty())
                return;
            // create session for each distribution we have found
            for (auto & distribution : distributions) {
                JSON session{JSON::Kind::Object};
                std::string pty = "local";
                session.setComment(STR("WSL distribution " << distribution));
                if (distribution == defaultSession)
                    session.setComment(session.comment() + " (default)");
                if (WSLIsBypassPresent(distribution)) {
                    pty = "bypass";
                } else {
                    if (Application::Instance()->query("ConPTY Bypass Installation", STR("Do you want to install the ConPTY bypass, which allows for faster I/O and has full support for ANSI escape sequences into WSL distribution " << distribution))) 
                        if (WSLInstallBypass(distribution)) {
                            pty = "bypass";
                        } else {
                            Application::Instance()->alert("Bypass installation failed, most likely due to missing binary for your WSL distribution. Terminal++ will continue with ConPTY, you can install the bypass manually later");
                        }
                }
                session.add("name", JSON{distribution});
                session.add("pty", JSON{pty});
                session.add("workingDirectory", JSON{HomeDir()});
                if (pty == "local")
                    session.add("command", JSON::Parse(STR("[\"wsl.exe\", \"--distribution\", \"" << distribution << "\"]")));
                else
                    session.add("command", JSON::Parse(STR("[\"wsl.exe\", \"--distribution\", \"" << distribution << "\", \"--\", \"" << BYPASS_PATH << "\"]")));
                sessions.add(session);
            }
            // update the default session name at last only if the wsl installation process was without errors
            defaultSessionName = defaultSession;
        } catch (OSError const & e) {
            // do nothing, when we get os error from calling WSL just don't include any sessions
        }
    }

    void Config::Win32AddMsys2(JSON & sessions, std::string & defaultSessionName) {
        try {
            ExitCode ec; // to silence errors
            Exec(Command{"C:\\msys64\\msys2_shell.cmd", {"--help"}}, &ec);
            // if there is no msys2, return now, otherwise add the sessions
            if (ec != EXIT_SUCCESS)
                return;
            JSON mingw64{JSON::Kind::Object};
            mingw64.setComment("msys2 - mingw64");
            mingw64.add("name", JSON{"mingw64 (msys2)"});
            mingw64.add("workingDirectory", JSON{STR("C:\\msys64\\home\\" << GetUsername())});
            mingw64.add("command", JSON::Parse("[\"C:\\\\msys64\\\\msys2_shell.cmd\",\"-defterm\",\"-here\",\"-no-start\",\"-mingw64\"]"));
            sessions.add(mingw64);
            JSON mingw32{JSON::Kind::Object};
            mingw32.setComment("msys2 - mingw32");
            mingw32.add("name", JSON{"mingw32 (msys2)"});
            mingw32.add("workingDirectory", JSON{STR("C:\\msys64\\home\\" << GetUsername())});
            mingw32.add("command", JSON::Parse("[\"C:\\\\msys64\\\\msys2_shell.cmd\",\"-defterm\",\"-here\",\"-no-start\",\"-mingw32\"]"));
            sessions.add(mingw32);
            JSON msys{JSON::Kind::Object};
            msys.setComment("msys2 - msys");
            msys.add("name", JSON{"msys (msys2)"});
            msys.add("workingDirectory", JSON{STR("C:\\msys64\\home\\" << GetUsername())});
            msys.add("command", JSON::Parse("[\"C:\\\\msys64\\\\msys2_shell.cmd\",\"-defterm\",\"-here\",\"-no-start\",\"-msys\"]"));
            sessions.add(msys);
        } catch (OSError const & e) {
            // do nothing, when we get os error from calling WSL just don't include any sessions
        }
    }

    #endif

} // namespace tpp
