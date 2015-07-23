//
//  FileDelete.h
//  ToBeHero
//
//  Created by darklinden on 11/7/14.
//
//

#ifndef __ToBeHero__FileDelete__
#define __ToBeHero__FileDelete__

#include "cocos2d.h"
#include <stdio.h>
#include <sys/stat.h>

#if (CC_PLATFORM_IOS == CC_TARGET_PLATFORM) || (CC_PLATFORM_ANDROID == CC_TARGET_PLATFORM)
#include <unistd.h>
#include <dirent.h>
#endif

#if CC_PLATFORM_WIN32 == CC_TARGET_PLATFORM
#include <Windows.h>
#endif

class FileUtilsExtension {
    
public:
    
    static bool delete_file(const std::string& fileName);
    
    static int path_is_directory(const std::string& path);
    
    static bool delete_folder_tree(const std::string& directory_name);
    
};

#endif /* defined(__ToBeHero__FileDelete__) */
