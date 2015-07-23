//
//  FileDelete.cpp
//  ToBeHero
//
//  Created by darklinden on 11/7/14.
//
//

#include "FileUtilsExtension.h"

#if (CC_PLATFORM_IOS == CC_TARGET_PLATFORM) || (CC_PLATFORM_ANDROID == CC_TARGET_PLATFORM)
int FileUtilsExtension::path_is_directory(const std::string& path) {
    struct stat s_buf;
    
    if (stat(path.c_str(), &s_buf))
        return 0;
    
    return S_ISDIR(s_buf.st_mode);
}

bool FileUtilsExtension::delete_folder_tree(const std::string& directory_name) {
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
}
#endif

#if CC_PLATFORM_WIN32 == CC_TARGET_PLATFORM
int FileUtilsExtension::path_is_directory(const std::string& path) {
    struct stat s_buf;
    
    if (stat(path.c_str(), &s_buf))
        return 0;
    
    return ( s_buf.st_mode & S_IFDIR );
}
#endif

bool FileUtilsExtension::delete_file(const std::string& fileName) {
    
    std::string path = cocos2d::FileUtils::getInstance()->fullPathForFilename(fileName);
    
#if (CC_TARGET_PLATFORM != CC_PLATFORM_WIN32)
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
    if (cocos2d::FileUtils::getInstance()->isFileExist(path))
    {
        if (FileUtilsExtension::path_is_directory(path.c_str())) {
            return RemoveDirectoryA(path.c_str());
        }
        else {
            return DeleteFileA(path.c_str());
        }
    }
    else {
        return false;
    }
#endif
    
};
