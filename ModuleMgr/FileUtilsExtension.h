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
#include <unistd.h>
#include <dirent.h>
#include <vector>

class FileUtilsExtension {
    
public:
    
    static bool delete_file(const std::string& fileName);
    
    static int path_is_directory(const std::string& path);
    
    static bool delete_folder_tree(const std::string& directory_name);
    
    static std::vector<std::string> content_of_folder(const std::string& path);
};

#endif /* defined(__ToBeHero__FileDelete__) */
