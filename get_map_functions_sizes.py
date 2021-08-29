#!/usr/bin/env python3

import argparse
import math
import os
import re
import collections
from typing import List

regex_fileDataEntry = re.compile(r"(?P<vram>0x[^\s]+)\s+(?P<size>0x[^\s]+)\s+(?P<name>[^\s]+)$")
regex_functionEntry = re.compile(r"(?P<vram>0x[^\s]+)\s+(?P<name>[^\s]+)$")

File = collections.namedtuple("File", ["name", "vram", "size", "functions"])
Function = collections.namedtuple("Function", ["name", "vram", "size"])

def parseMapFile(mapPath: str) -> List[File]:
    with open(mapPath) as f:
        mapData = f.read()
        startIndex = mapData.find("..makerom")
        mapData = mapData[startIndex:]
    # print(len(mapData))

    filesList: List[File] = list()

    inFile = False
    currentFileName = ""
    currentFileSize = 0
    currentFileVram = 0

    mapLines = mapData.split("\n")
    for line in mapLines:
        if inFile:
            if line.startswith("                "):
                entryMatch = regex_functionEntry.search(line)

                if entryMatch is not None:
                    funcName = entryMatch["name"]
                    funcVram = int(entryMatch["vram"], 16)
                    filesList[-1].functions.append(Function(funcName, funcVram, -1))
                    # print(hex(funcVram), funcName)

            else:
                inFile = False
        else:
            if line.startswith(" .text "):
                inFile = False
                entryMatch = regex_fileDataEntry.search(line)

                if entryMatch is not None:
                    currentFileName = "/".join(entryMatch["name"].split("/")[2:])
                    currentFileName = ".".join(currentFileName.split(".")[:-1])
                    currentFileSize = int(entryMatch["size"], 16)
                    currentFileVram = int(entryMatch["vram"], 16)
                    if currentFileSize > 0:
                        inFile = True
                        #filesList.append({"name": currentFileName, "size": currentFileSize, "vram": currentFileVram, "functions": list()})
                        filesList.append(File(currentFileName, currentFileVram, currentFileSize, list()))
                        # print(currentFileName, hex(currentFileSize), hex(currentFileVram))

    for file in filesList:
        accummulatedSize = 0
        funcCount = len(file.functions)
        if funcCount == 0:
            continue

        for index in range(funcCount-1):
            func = file.functions[index]
            nextFunc = file.functions[index+1]

            size = nextFunc.vram - func.vram
            accummulatedSize += size

            file.functions[index] = Function(func.name, func.vram, size)
            #print(file.functions[index])

        func = file.functions[funcCount-1]
        size = file.size - accummulatedSize
        file.functions[funcCount-1] = Function(func.name, func.vram, size)

    for file in filesList:
        funcCount = len(file.functions)
        for index in range(funcCount):
            func = file.functions[index]
            #print(func)

    return filesList

def printCsv(filesList: List[File]):
    print("VRAM,File,Num functions,Max size,Total size,Average size")

    for file in filesList:
        #print(file)
        name = file.name
        vram = file.vram
        size = file.size
        funcCount = len(file.functions)

        if funcCount == 0:
            continue

        maxSize = 0
        for func in file.functions:
            funcSize = func.size
            if funcSize > maxSize:
                maxSize = funcSize

        print(f"{vram:08X},{name},{funcCount},{maxSize},{size},{size/funcCount:0.2f}")



def main():
    description = "Produces a csv summarizing the files sizes by parsing a map file."
    # TODO
    epilog = """\
    """

    parser = argparse.ArgumentParser(description=description, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("mapfile", help="Path to a map file.")
    # parser.add_argument("--non-matching", help="Collect data of the non-matching actors instead.", action="store_true")
    #parser.add_argument("--function-lines", help="Prints the size of every function instead of a summary.", action="store_true")
    #parser.add_argument("--ignore", help="Path to a file containing actor's names. The data of actors in this list will be ignored.")
    #parser.add_argument("--include-only", help="Path to a file containing actor's names. Only data of actors in this list will be printed.")
    args = parser.parse_args()

    filesList = parseMapFile(args.mapfile)

    printCsv(filesList)

if __name__ == "__main__":
    main()
