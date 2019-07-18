#include <cstdlib>
#include <iostream>
#include <fstream>

#include "helpers/stamp.h"
#include "helpers/args.h"


int main(int argc, char* argv[]) {
	try {
		if (argc != 3)
			THROW(helpers::Exception()) << "Invalid number of arguments";
		std::string path = argv[1];
		std::string stampFile = path + "/" + argv[2];

		helpers::Stamp stamp = helpers::Stamp::FromGit(path);

		std::cout << "Created stamp: " << stamp << std::endl;
		std::cout << "Writing to " << stampFile << std::endl;

		std::ofstream f(stampFile);
		f << "/* AUTOGENERATED FILE, DO NOT EDIT! \n";
		f << "   This file was produced by the following command:\n";
		f << "   " << helpers::Arguments::Print(argc, argv) << "\n";
		f << "*/\n";
		stamp.outputCppMacro(f);
		f << std::endl;
		f.close();

		return EXIT_SUCCESS;
	} catch (helpers::Exception const& e) {
		std::cerr << e << std::endl;
	} catch (std::exception const& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Error." << std::endl;
	}
	return EXIT_FAILURE;
}

