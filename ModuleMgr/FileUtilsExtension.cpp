//
//  FileDelete.cpp
//  ToBeHero
//
//  Created by darklinden on 11/7/14.
//
//

#include "FileUtilsExtension.h"

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

bool FileUtilsExtension::delete_file(const std::string& fileName) {
    
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
    
};

std::vector<std::string> FileUtilsExtension::content_of_folder(const std::string& path)
{
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
}