#!/bin/bash
rootBuildPath=TMSS
export objectFileSet
export sourceFileExtension
export libraryFileSet

echo "=====Build C/C++ using gcc compiler====="

if [ -d tinyMediaStreamingServer ]; then
	echo "tinyMediaStreamingServer directory already exists"
else
	$(mkdir tinyMediaStreamingServer)
fi

if [ -d temp ]; then
	echo "temp directory already exists"
	
	$(rm -rf temp/)
	$(mkdir temp)	
else
	$(mkdir temp)
fi

# Compile the files
sourceFileSet=$(ls ${rootBuildPath}/sources/*)

for sourceFile in ${sourceFileSet[@]}; do
	objectFileName=$(basename ${sourceFile})
	objectNameWithoutExtension="${objectFileName%.*}"
	sourceFileExtension="${objectFileName##*.}"

	if [ ${sourceFileExtension} == "cpp" ]; then
		if [ -d ${rootBuildPath}/headers ]; then
			g++ -c ${sourceFile} -I ${rootBuildPath}/headers/
		else	
			g++ -c ${sourceFile}
		fi
	else
		if [ -d ${rootBuildPath}/headers ]; then
			gcc -c ${sourceFile} -I ${rootBuildPath}/headers/
		else	
			gcc -c ${sourceFile}
		fi
	fi
	mv ${objectNameWithoutExtension}.o temp/

	objectFileSet="${objectFileSet} temp/${objectNameWithoutExtension}.o"
done

if [ -d ${rootBuildPath}/libs ]; then
	libSet=$(ls ${rootBuildPath}/libs/*)
	for library in ${libSet[@]}; do
		libraryName=$(basename ${library})
		libraryNameWithoutExtension="${libraryName%.*}"

		libName=${libraryNameWithoutExtension:3}
		libraryFileSet="${libraryFileSet} -l${libName}"
	done
fi

if [ ${sourceFileExtension} == 'cpp' ]; then
	g++ $libraryFileSet $objectFileSet -o tinyMediaStreamingServer/TMSS
else
	gcc $libraryFileSet $objectFileSet -o tinyMediaStreamingServer/TMSS
fi

$(rm -rf temp/)

echo "===DONE==="