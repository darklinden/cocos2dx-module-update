//
//  FileDelete.cpp
//  ToBeHero
//
//  Created by darklinden on 11/7/14.
//
//

#include "FileUtilsExtension.h"

#if CC_TARGET_PLATFORM == CC_PLATFORM_IOS

int FileUtilsExtension::path_is_directory(const std::string& path) {
#ifndef WIN32
	struct stat s_buf;

	if (stat(path.c_str(), &s_buf))
		return 0;

	return S_ISDIR(s_buf.st_mode);
#else
	DWORD dwAttrs = GetFileAttributes((LPCWSTR)path.c_str());
	if (dwAttrs == INVALID_FILE_ATTRIBUTES) return false;

	if (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) {
		return 1;
	}

	return 0;
#endif
}

bool FileUtilsExtension::delete_folder_tree(const std::string& directory_name) {
#ifndef WIN32
    DIR*            dp;
    struct dirent*  ep;
    
    dp = opendir(directory_name.c_str());
    
    while ((ep = readdir(dp)) != NULL) {
        if (std::string(ep->d_name) == "." || std::string(ep->d_name) == "..") continue;
        
        std::string p_buf = directory_name + "/" + ep->d_name; //"%s/%s"
        if (path_is_directory(p_buf))
            delete_folder_tree(p_buf);
        else
            unlink(p_buf.c_str());
    }
    
    closedir(dp);
    return (0 == rmdir(directory_name.c_str()));
#else
	BOOL ret = RemoveDirectory((LPCWSTR)directory_name.c_str());
	if (!ret) {
		cocos2d::log("FileUtilsExtension::delete_folder_tree failed with error %ld", GetLastError());
	}

	return !!ret;
#endif
}

bool FileUtilsExtension::delete_file(const std::string& fileName) {
#ifndef WIN32
    std::string path = cocos2d::FileUtils::getInstance()->fullPathForFilename(fileName);
    
    if (cocos2d::FileUtils::getInstance()->isFileExist(path))
    {
        if (FileUtilsExtension::path_is_directory(path)) {
            //is dir
            return FileUtilsExtension::delete_folder_tree(path);
        }
        else {
            //is file
            return (0 == remove(path.c_str()));
        }
    }
    else {
        return false;
    }
#else
	
	BOOL ret = DeleteFile((LPCWSTR)fileName.c_str());
	if (!ret) {
		cocos2d::log("FileUtilsExtension::delete_folder_tree failed with error %ld", GetLastError());
	}

	return !!ret;
#endif
};

std::vector<std::string> FileUtilsExtension::content_of_folder(const std::string& path)
{
#ifndef WIN32
	std::vector<std::string> ret;
	ret.clear();

	if (!path_is_directory(path)) {
		return ret;
	}

    DIR*            dp;
    struct dirent*  ep;
    
    dp = opendir(path.c_str());
    
    while ((ep = readdir(dp)) != NULL) {
        if (std::string(ep->d_name) == "." || std::string(ep->d_name) == "..") continue;
        ret.push_back(std::string(ep->d_name));
    }
    
    closedir(dp);
    return ret;
#else
	std::vector<std::string> ret;
	ret.clear();
	return ret;

	/*
	if (!path_is_directory(path)) {
		return ret;
	}

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	hFind = FindFirstFile((LPCWSTR)path.c_str(), &FindFileData);
	while (hFind != INVALID_HANDLE_VALUE)
	{
		ret.push_back(std::wstring(FindFileData.cFileName).c_str());
		hFind = FindNextFile((LPCWSTR)path.c_str(), &FindFileData);
		if (0 == hFind)
		{

		}
	}
	else
	{
		_tprintf(
			TEXT("The first file found is %s\n"),
			FindFileData.cFileName);
		FindClose(hFind);
	}*/

#endif
}

void FileUtilsExtension::skipiCloudBackupForItemAtPath(const std::string& path)
{
    NSString* filePath = [NSString stringWithUTF8String:path.c_str()];
    
    if (![[NSFileManager defaultManager] fileExistsAtPath:filePath]) {
        return;
    }
    
    NSURL* URL = [NSURL fileURLWithPath:filePath];
    if (URL) {
        [URL setResourceValue:[NSNumber numberWithBool:YES]
                       forKey:NSURLIsExcludedFromBackupKey
                        error:nil];
    }
}

#endif