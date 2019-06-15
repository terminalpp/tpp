#define DEFAULT_TERMINAL_COLS 80
#define DEFAULT_TERMINAL_ROWS 25
#define DEFAULT_TERMINAL_FONT_HEIGHT 18

#ifdef WIN32

//#define DEFAULT_SESSION_COMMAND helpers::Command("wsl", {"-e", "bash"})
#define DEFAULT_SESSION_COMMAND helpers::Command("wsl", {"-e", "/home/peta/devel/tpp-build/asciienc/asciienc", "bashus"})
//#define DEFAULT_SESSION_COMMAND helpers::Command("wsl", {"--help"})

#elif __linux__

//#define DEFAULT_SESSION_COMMAND helpers::Command("bash", {})
#define DEFAULT_SESSION_COMMAND helpers::Command("/home/peta/devel/tpp-build/asciienc/asciienc", {"mc"})

#endif