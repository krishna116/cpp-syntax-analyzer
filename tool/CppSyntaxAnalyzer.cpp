/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "config.h"
#include "miniopt.h"
#include "GrammarContextBuilder.h"
#include "LL1Analyzer.h"
#include <iostream>
#include <fstream>

using namespace csa;

int StreamToFile(const std::string& stream, std::string filename){
    if(stream.empty() || filename.empty()){
        return 1;
    }

    std::string suffix = ".html";
    auto suffixSize = suffix.size();
    auto filenameSize = filename.size();
    if(filenameSize < suffixSize || filename.substr(filenameSize - suffixSize) != suffix){
        filename += suffix;
    }

    std::ofstream ofs(filename);
    if(ofs){
        ofs << stream;
        ofs.flush();
        ofs.close();
    }else{
        printf("error: cannot write file = %s\n", filename.c_str());
        return 1;
    }

    return 0;
}

std::string ReadStreamFromStdin(){
    std::string stream;
    std::string line;
    while(std::getline(std::cin,line)){
        stream += line + "\n";
    }
    return stream;
}

int DoWork(std::string in, std::string out){
    GrammarContextPtr gc;

    if(in.empty()){
        auto stream = ReadStreamFromStdin();
        gc = GrammarContextBuilder::buildFromStream(stream);
    }else{
        gc = GrammarContextBuilder::buildFromFile(in);
    }

    if(gc){
        LL1Analyzer theLL1Analyzer(gc);
        if(theLL1Analyzer.parse() == 0){
            auto stream = theLL1Analyzer.buildHtmlTable();
            if(!out.empty()){
                return StreamToFile(stream, out);
            }else{
                std::cout << stream << std::endl;
                return 0;
            }
        }
    }

    return 1;
}

int ParseArgs(int argc, char *argv[]) {
    option options[] = {
        {'o', "out", "<file>", ""},
        {'v', "version", nil, ""},
        {'h', "help", nil, ""}
    };
    const int optsum = sizeof(options) / sizeof(options[0]);

    if (miniopt.init(argc, (char **)argv, options, optsum) != 0) {
        printf("error: %s\n", miniopt.what());
        return 1;
    }

    std::string in;
    std::string out;
    int status;
    while ((status = miniopt.getopt()) > 0) {
        int id = miniopt.optind();
        switch (id) {
            case 0: // -o --out <file>
                out = miniopt.optarg();
            break;
            case 1: // -v --version
                std::cout << config::VersionStr << "\n";
                return 0;
            case 2: // -h --help
                std::cout << config::HelpStr << "\n";
                return 0;
            default:
            if(in.empty()){ in = miniopt.optarg(); }
            break;
        }
    }

    if (status < 0){
        printf("error: %s\n", miniopt.what());
        return status;
    }

    return DoWork(in, out);
}

int main(int argc, char* argv[]){
    return ParseArgs(argc, argv);
}
