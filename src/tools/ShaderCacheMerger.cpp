#include "Cemu/FileCache/FileCache.h"
#include "Cafe/HW/Latte/Core/LatteShaderCache.h"
#include <regex>
#include <cinttypes>

void MergeShaderCacheFile(std::string fileName)
{
	// parse titleId from fileName
	uint64 titleId = 0;
	if (sscanf(fileName.c_str(), "%" SCNx64, &titleId) != 1)
		return;

	const std::string mainPath = "shaderCache/transferable/" + fileName;
	const std::string mergeSourcePath = "shaderCache/transferable/merge/" + fileName;
	if (!fs::exists(mainPath) || !fs::exists(mergeSourcePath))
		return;
	// open both caches
	FileCache* mainCache = FileCache::Open(boost::nowide::widen(mainPath));
	FileCache* sourceCache = FileCache::Open(boost::nowide::widen(mergeSourcePath));
	if (!mainCache && !sourceCache)
	{
		printf("Failed to open cache files for %s\n", fileName.c_str());
		if (mainCache)
			delete mainCache;
		if (sourceCache)
			delete sourceCache;
		return;
	}
	// begin merging
	printf("Merging shaders %" PRIx64 "...", titleId);
	uint32 numMergedEntries = 0; // number of files added to the main cache file
	for (sint32 i = 0; i < sourceCache->GetFileCount(); i++)
	{
		uint64 name1, name2;
		std::vector<uint8> fileData;
		if (!sourceCache->GetFileByIndex(i, &name1, &name2, fileData))
			continue;
		std::vector<uint8> existingfileData;
		if (mainCache->HasFile({ name1, name2 }))
			continue;
		mainCache->AddFile({ name1, name2 }, fileData.data(), (sint32)fileData.size());
		numMergedEntries++;
	}
	printf(" -> Added %d new shaders for a total of %d\n", numMergedEntries, mainCache->GetFileCount());

	delete mainCache;
	delete sourceCache;

	fs::remove(mergeSourcePath);
}

void MergePipelineCacheFile(std::string fileName)
{
	// parse titleId from fileName
	uint64 titleId = 0;
	if (sscanf(fileName.c_str(), "%" SCNx64, &titleId) != 1)
		return;

	const std::string mainPath = "shaderCache/transferable/" + fileName;
	const std::string mergeSourcePath = "shaderCache/transferable/merge/" + fileName;
	if (!fs::exists(mainPath) || !fs::exists(mergeSourcePath))
		return;
	// open both caches
	const uint32 cacheFileVersion = 1;
	FileCache* mainCache = FileCache::Open(boost::nowide::widen(mainPath));
	FileCache* sourceCache = FileCache::Open(boost::nowide::widen(mergeSourcePath));
	if (!mainCache && !sourceCache)
	{
		printf("Failed to open cache files for %s\n", fileName.c_str());
		if (mainCache)
			delete mainCache;
		if (sourceCache)
			delete sourceCache;
		return;
	}
	// begin merging
	printf("Merging pipelines %" PRIx64 "...", titleId);
	uint32 numMergedEntries = 0; // number of files added to the main cache file
	for (sint32 i = 0; i < sourceCache->GetFileCount(); i++)
	{
		uint64 name1, name2;
		std::vector<uint8> fileData;
		if (!sourceCache->GetFileByIndex(i, &name1, &name2, fileData))
			continue;
		std::vector<uint8> existingfileData;
		if (mainCache->HasFile({ name1, name2 }))
			continue;
		mainCache->AddFile({ name1, name2 }, fileData.data(), (sint32)fileData.size());
		numMergedEntries++;
	}
	printf(" -> Added %d new pipelines for a total of %d\n", numMergedEntries, mainCache->GetFileCount());

	delete mainCache;
	delete sourceCache;

	fs::remove(mergeSourcePath);
}

void MergeShaderAndPipelineCacheFiles()
{
	printf("Scanning for shader cache files to merge...\n");
	for (const auto& it : fs::directory_iterator("shaderCache/transferable/"))
	{
		if (fs::is_directory(it))
			continue;
		auto filename = it.path().filename().generic_string();
		if (std::regex_match(filename, std::regex("^[0-9a-fA-F]{16}(?:_shaders.bin)")))
			MergeShaderCacheFile(filename);
	}
	printf("\nScanning for pipeline cache files to merge...\n");
	for (const auto& it : fs::directory_iterator("shaderCache/transferable/"))
	{
		if (fs::is_directory(it))
			continue;
		auto filename = it.path().filename().generic_string();
		if (std::regex_match(filename, std::regex("^[0-9a-fA-F]{16}(?:_vkpipeline.bin)")))
			MergePipelineCacheFile(filename);
	}
}

void ToolShaderCacheMerger()
{
	printf("****************************************************\n");
	printf("****************************************************\n");
	printf("Shader and pipeline cache merging tool\n");
	printf("This tool will merge any shader caches placed in:\n");
	printf("shaderCache/transferable/merge/\n");
	printf("into the files of the same name in:\n");
	printf("shaderCache/transferable/\n");
	printf("****************************************************\n");
	printf("****************************************************\n");
	printf("\n");

	MergeShaderAndPipelineCacheFiles();


	printf("done!\n");
	while (true)
		Sleep(1000);
	exit(0);
}
