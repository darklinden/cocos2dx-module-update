//
//  main.m
//  jsonmaker
//
//  Created by HanShaokun on 23/7/15.
//  Copyright (c) 2015 by. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>

#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <stdio.h>
#include <iostream>
#include <iomanip>

static __strong NSString* _srcFolderPath = nil;

static __strong NSString* _desFolderPath = nil;
static __strong NSMutableDictionary *_desManifestDict = nil;
static __strong NSString* _desManifestPath = nil;

static __strong NSString* _manifest_name = @"project.manifest";
static __strong NSString* _config_name = @"jsonmaker.config";

static __strong NSString* _param_key = @"cmd-param";

static void readManifest()
{
    // read des
    if (_desFolderPath) {
        _desManifestPath = [_desFolderPath stringByAppendingPathComponent:_manifest_name];
    }
    
    if (_desManifestPath.length) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:_desManifestPath]) {
            
            NSString* content = [NSString
                                 stringWithContentsOfFile:_desManifestPath
                                 encoding:NSUTF8StringEncoding
                                 error:nil];
            
            if (content.length) {
                NSDictionary* jdict = [NSJSONSerialization
                                       JSONObjectWithData:[content dataUsingEncoding:NSUTF8StringEncoding]
                                       options:0
                                       error:nil];
                
                _desManifestDict = [NSMutableDictionary dictionaryWithDictionary:jdict];
            }
        }
        
        else {
            _desManifestDict = [NSMutableDictionary dictionary];
        }
    }
}

static void saveManifest() {
    if (_desManifestDict.count) {
        NSError *error;
        NSData *jsonData = [NSJSONSerialization
                            dataWithJSONObject:_desManifestDict
                            options:NSJSONWritingPrettyPrinted
                            error:&error];
        if (!jsonData) {
            printf("manifest save json failed %s\n", error.description.UTF8String);
        }
        else {
            if (![jsonData writeToFile:_desManifestPath atomically:YES]) {
                printf("manifest save json failed\n");
            }
        }
    }
}

static NSMutableDictionary* fileDict(long long              len,
                                     BOOL                   compressed,
                                     long long              timestamp,
                                     NSMutableDictionary*   content)
{
    NSMutableDictionary* filedict = [NSMutableDictionary dictionary];
    filedict[@"len"] = @(len);
    filedict[@"timestamp"] = @(timestamp);
    if (compressed) {
        filedict[@"compressed"] = @(compressed);
    }
    if (content) {
        filedict[@"content"] = content;
    }
    
    return filedict;
}

static long long fileGitTime(NSString* filepath)
{
    // git log --follow -1 --format=%at -- path
    // int pid = [[NSProcessInfo processInfo] processIdentifier];
    NSString* path = [filepath copy];
    
    if ([path.pathExtension.lowercaseString isEqualToString:@"luac"]) {
        path = [[path stringByDeletingPathExtension] stringByAppendingPathExtension:@"lua"];
    }
    
    NSPipe *pipe = [NSPipe pipe];
    NSFileHandle *file = pipe.fileHandleForReading;
    
    NSTask *task = [[NSTask alloc] init];
    
    task.launchPath = @"/usr/bin/git";
    task.arguments = @[@"log", @"--follow", @"-1", @"--format=%at", @"--", path];
    task.standardOutput = pipe;
    task.currentDirectoryPath = [path stringByDeletingLastPathComponent];
    
    [task launch];
    
    NSData *data = [file readDataToEndOfFile];
    [file closeFile];
    
    NSString *grepOutput = [[NSString alloc] initWithData:data
                                                 encoding:NSUTF8StringEncoding];
    long long ret = [grepOutput longLongValue];
    printf("fileGitTime: %s - %lld\n", filepath.UTF8String, ret);
    return ret;
}

static void setFileTime(NSString* filePath, long long timestamp) {
    struct stat attr;
    
    stat(filePath.UTF8String, &attr);
    
    struct utimbuf new_times;
    new_times.actime = attr.st_atime; /* keep atime unchanged */
    new_times.modtime = timestamp;    /* set mtime to current time */
    utime(filePath.UTF8String, &new_times);
    
    NSMutableDictionary* mattr = [NSMutableDictionary dictionaryWithDictionary:
                                  [[NSFileManager defaultManager] attributesOfItemAtPath:filePath error:nil]];
    
    [mattr setObject:[NSDate dateWithTimeIntervalSince1970:timestamp]
              forKey:NSFileCreationDate];
}

static void luaCompile()
{
    // cocos luacompile -s src -d src -e -k '' -b BYE --disable-compile
    NSString* confPath = [[_srcFolderPath stringByDeletingLastPathComponent] stringByAppendingPathComponent:_config_name];
    NSString* content = [NSString stringWithContentsOfFile:confPath encoding:NSUTF8StringEncoding error:nil];
    
    NSDictionary* jdict = [NSJSONSerialization
                           JSONObjectWithData:[content dataUsingEncoding:NSUTF8StringEncoding]
                           options:0
                           error:nil];
    
    NSString* param = jdict[_param_key];
    NSString* COCOS_CONSOLE_ROOT = jdict[@"COCOS_CONSOLE_ROOT"];
    
    NSString* cmd = [COCOS_CONSOLE_ROOT stringByAppendingPathComponent:@"cocos"];
    
    cmd = [[[[cmd stringByAppendingString:@" luacompile -s "]
             stringByAppendingString:_srcFolderPath]
            stringByAppendingString:@" -d "]
           stringByAppendingString:_srcFolderPath];
    
    if (param.length) {
        cmd = [[cmd stringByAppendingString:@" "] stringByAppendingString:param];
    }

    printf("execute: %s\n", cmd.UTF8String);
    system(cmd.UTF8String);
}

static NSString* pwd()
{
    // nothing to commit, working directory clean
    NSPipe *pipe = [NSPipe pipe];
    NSFileHandle *file = pipe.fileHandleForReading;
    
    NSTask *task = [[NSTask alloc] init];
    
    task.launchPath = @"/bin/pwd";
    task.arguments = @[];
    task.standardOutput = pipe;
    
    [task launch];
    
    NSData *data = [file readDataToEndOfFile];
    [file closeFile];
    
    NSString *grepOutput = [[NSString alloc] initWithData:data
                                                 encoding:NSUTF8StringEncoding];
    return [grepOutput stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
}

static NSString* checkGit()
{
    // nothing to commit, working directory clean
    NSPipe *pipe = [NSPipe pipe];
    NSFileHandle *file = pipe.fileHandleForReading;
    
    NSTask *task = [[NSTask alloc] init];
    
    task.launchPath = @"/usr/bin/git";
    task.arguments = @[@"status", @"-s"];
    task.standardOutput = pipe;
    task.currentDirectoryPath = _srcFolderPath;
    
    [task launch];
    
    NSData *data = [file readDataToEndOfFile];
    [file closeFile];
    
    NSString *grepOutput = [[NSString alloc] initWithData:data
                                                 encoding:NSUTF8StringEncoding];
    return grepOutput;
}

static void mkSrcAssets(NSArray *excludes) {
    
    NSFileManager *fmgr = [NSFileManager defaultManager];
    NSError *err = nil;
    
    NSString* folder = _srcFolderPath;
    NSArray* subfiles = [fmgr subpathsOfDirectoryAtPath:folder error:&err];
    
    NSString* upFolder = [_srcFolderPath stringByDeletingLastPathComponent];
    
    NSString* desPath = [[_desFolderPath stringByAppendingPathComponent:_srcFolderPath.lastPathComponent] stringByAppendingPathExtension:@"zip"];

    int cnt = 0;
    for (NSString* name in subfiles) {
        cnt += 1;
        if ([name.lastPathComponent isEqualToString:@".DS_Store"]) {
            continue;
        }
        
        if ([name hasPrefix:@".git"]) {
            continue;
        }
        
        if ([name.lastPathComponent isEqualToString:_manifest_name]) {
            printf("exclude: %s\n", name.UTF8String);
            continue;
        }
        
        if ([name.pathExtension.lowercaseString isEqualToString:@"lua"]) {
            printf("exclude: %s\n", name.UTF8String);
            continue;
        }
        
        BOOL ex = NO;
        for (int i = 0; i < excludes.count; i++) {
            if ([name hasPrefix:excludes[i]]) {
                printf("exclude: %s\n", name.UTF8String);
                ex = YES;
                break;
            }
            if ([[folder stringByAppendingPathComponent:name] hasPrefix:excludes[i]]) {
                printf("exclude: %s\n", name.UTF8String);
                ex = YES;
                break;
            }
        }
        if (ex) {
            continue;
        }
        
        NSString* full = [folder stringByAppendingPathComponent:name];
        
        BOOL isDirectory = false;
        BOOL exist = [fmgr fileExistsAtPath:full
                                isDirectory:&isDirectory];
        if (isDirectory && exist) {
            continue;
        }
        
        setFileTime(full, fileGitTime(full));
        
        NSMutableString* cmd = [NSMutableString stringWithFormat:@"cd %@ && zip %@ ", upFolder, desPath];
        [cmd appendString:[[_srcFolderPath lastPathComponent] stringByAppendingPathComponent:name]];
        printf("execute: %s\n", cmd.UTF8String);
        system(cmd.UTF8String);

        std::cout << std::fixed;
        std::cout.precision(2);

        std::cout << "                                      \r";
        std::cout.flush();

        std::cout << "progress: " << float(cnt * 100.0 / subfiles.count) << " % \r";
        std::cout.flush();
    }
    printf("Done.\n");
}

static void addAssets(NSArray *excludes) {
    
    NSFileManager *fmgr = [NSFileManager defaultManager];
    NSError *err = nil;
    
    NSString* folder = _srcFolderPath;
    NSArray* subfiles = [fmgr subpathsOfDirectoryAtPath:folder error:&err];
    
    NSMutableDictionary* dict = [NSMutableDictionary dictionary];
    
    for (NSString* name in subfiles) {
        
        if ([name.lastPathComponent isEqualToString:@".DS_Store"]) {
            continue;
        }
        
        if ([name hasPrefix:@".git"]) {
            continue;
        }
        
        if ([name.lastPathComponent isEqualToString:_manifest_name]) {
            printf("exclude: %s\n", name.UTF8String);
            continue;
        }
        
        if ([name.pathExtension.lowercaseString isEqualToString:@"lua"]) {
            printf("exclude: %s\n", name.UTF8String);
            continue;
        }
        
        BOOL ex = NO;
        for (int i = 0; i < excludes.count; i++) {
            if ([name hasPrefix:excludes[i]]) {
                printf("exclude: %s\n", name.UTF8String);
                ex = YES;
                break;
            }
            if ([[folder stringByAppendingPathComponent:name] hasPrefix:excludes[i]]) {
                printf("exclude: %s\n", name.UTF8String);
                ex = YES;
                break;
            }
        }
        if (ex) {
            continue;
        }
        
        BOOL isDirectory = false;
        BOOL exist = [fmgr fileExistsAtPath:[folder stringByAppendingPathComponent:name]
                                isDirectory:&isDirectory];
        if (isDirectory && exist) {
            continue;
        }
        
        NSArray* keys = [name componentsSeparatedByString:@"/"];
        NSString* key = [[keys firstObject] stringByAppendingPathExtension:@"zip"];
        
        NSMutableArray* subfiles = [NSMutableArray arrayWithArray:dict[key]];
        
        [subfiles addObject:name];
        
        [dict setObject:subfiles forKey:key];
    }
    
    NSString* upFolder = [_srcFolderPath stringByDeletingLastPathComponent];
    
    for (NSString* key in [dict allKeys]) {
        
        NSArray* array = dict[key];
        
        NSMutableDictionary* subdict = [NSMutableDictionary dictionary];
        
        NSString* desPath = [_desFolderPath stringByAppendingPathComponent:key];
        
        [fmgr removeItemAtPath:desPath error:nil];
        
        NSMutableString* cmd = [NSMutableString stringWithFormat:@"cd %@ && zip -q %@ ", upFolder, desPath];

        printf("deal with %s\n", key.UTF8String);

        int cnt = 0;

        for (NSString* name in array) {
            cnt += 1;
            NSString* full = [folder stringByAppendingPathComponent:name];
            
            NSDictionary* attr = [fmgr attributesOfItemAtPath:full error:nil];
            
            long long timestamp = fileGitTime(full);
            
            NSDictionary* fdict = fileDict([attr[NSFileSize] longLongValue],
                                           false,
                                           timestamp,
                                           nil);
            
            [subdict setObject:fdict forKey:[[_srcFolderPath lastPathComponent] stringByAppendingPathComponent:name]];
            
            setFileTime(full, timestamp);
            
            [cmd appendString:[[_srcFolderPath lastPathComponent] stringByAppendingPathComponent:name]];
            [cmd appendString:@" "];

            std::cout << std::fixed;
            std::cout.precision(2);

            std::cout << "                                      \r";
            std::cout.flush();

            std::cout << "progress: " << float(cnt * 100.0 / subfiles.count) << " % \r";
            std::cout.flush();
        }
        
        NSLog(@"execute: %@", cmd);
        system(cmd.UTF8String);
        
        if (![fmgr fileExistsAtPath:desPath]) {
            NSLog(@"zip error with folder: %@\n", key);
            return;
        }
        
        NSDictionary* attr = [fmgr attributesOfItemAtPath:desPath error:nil];
        
        NSDictionary* fdict = fileDict([attr[NSFileSize] longLongValue],
                                       true,
                                       0,
                                       subdict);
        [dict setObject:fdict forKey:key];
    }
    
    [_desManifestDict setObject:dict forKey:@"assets"];
}

int main(int argc, const char * argv[]) {
    
    @autoreleasepool {
        
        // insert code here..
        if (argc < 2) {
            printf("\n使用 jsonmaker mkerc src-path des-path -ex exclude1 exclude2 ... 为源文件添加时间戳 assets 。\n");
            printf("使用 jsonmaker reset src-path des-path -ex exclude1 exclude2 ... 清空后重新添加 assets 。\n");
            return -1;
        }
        
        NSMutableArray *excludes = [NSMutableArray array];
        if (argc > 5) {
            if ([[NSString stringWithUTF8String:argv[4]] isEqualToString:@"-ex"]) {
                for (int i = 5; i < argc; i++) {
                    [excludes addObject:[NSString stringWithUTF8String:argv[i]]];
                }
            }
        }
        
        NSString* cmd = [NSString stringWithUTF8String:argv[1]];
        
        if ([cmd.lowercaseString isEqualToString:@"mkerc"]) {
            // mksrc ~/Desktop/src/project.manifest
            _srcFolderPath = [NSString stringWithUTF8String:argv[2]];
            _desFolderPath = [NSString stringWithUTF8String:argv[3]];
            
            if (![_srcFolderPath hasPrefix:@"/"] && ![_srcFolderPath hasPrefix:@"~"]) {
                _srcFolderPath = [pwd() stringByAppendingPathComponent:_srcFolderPath];
            }
            
            if (![_desFolderPath hasPrefix:@"/"] && ![_desFolderPath hasPrefix:@"~"]) {
                _desFolderPath = [pwd() stringByAppendingPathComponent:_desFolderPath];
            }
            
            NSLog(@"_srcFolderPath: %@", _srcFolderPath);
            NSLog(@"_desFolderPath: %@", _desFolderPath);
            
            luaCompile();
            mkSrcAssets(excludes);
        }
        else if ([cmd.lowercaseString isEqualToString:@"reset"]) {
            // reset ~/Developer/work/fish-lua-clear/src ~/Desktop/src
            _srcFolderPath = [NSString stringWithUTF8String:argv[2]];
            _desFolderPath = [NSString stringWithUTF8String:argv[3]];
            
            if (![_srcFolderPath hasPrefix:@"/"] && ![_srcFolderPath hasPrefix:@"~"]) {
                _srcFolderPath = [pwd() stringByAppendingPathComponent:_srcFolderPath];
            }
            
            if (![_desFolderPath hasPrefix:@"/"] && ![_desFolderPath hasPrefix:@"~"]) {
                _desFolderPath = [pwd() stringByAppendingPathComponent:_desFolderPath];
            }
            
            NSLog(@"_srcFolderPath: %@", _srcFolderPath);
            NSLog(@"_desFolderPath: %@", _desFolderPath);
            
            NSString* status = checkGit();
            if (status.length) {
                NSLog(@"git repository not clean:\n%@", status);
            }
            else {
                luaCompile();
                readManifest();
                addAssets(excludes);
                saveManifest();
            }
        }
        else {
            NSLog(@"\n无效的命令: %@\n", cmd);
        }
    }
    return 0;
}
