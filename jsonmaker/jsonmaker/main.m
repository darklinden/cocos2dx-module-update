//
//  main.m
//  jsonmaker
//
//  Created by HanShaokun on 23/7/15.
//  Copyright (c) 2015 by. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>

static __strong NSString* _srcFolderPath = nil;
static __strong NSMutableDictionary *_srcManifestDict = nil;
static __strong NSString* _srcManifestPath = nil;

static __strong NSString* _desFolderPath = nil;
static __strong NSMutableDictionary *_desManifestDict = nil;
static __strong NSString* _desManifestPath = nil;

static __strong NSString* _manifest_name = @"project.manifest";
static __strong NSString* _config_name = @"jsonmaker.config";

static __strong NSString* _param_key = @"cmd-param";

static void readManifest()
{
    // read src
    if (_srcFolderPath) {
        _srcManifestPath = [_srcFolderPath stringByAppendingPathComponent:_manifest_name];
    }
    
    if (_srcManifestPath.length) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:_srcManifestPath]) {
            
            NSString* content = [NSString
                                 stringWithContentsOfFile:_srcManifestPath
                                 encoding:NSUTF8StringEncoding
                                 error:nil];
            
            NSDictionary* jdict = [NSJSONSerialization
                                   JSONObjectWithData:[content dataUsingEncoding:NSUTF8StringEncoding]
                                   options:0
                                   error:nil];
            
            _srcManifestDict = [NSMutableDictionary dictionaryWithDictionary:jdict];
        }
        else {
            _srcManifestDict = [NSMutableDictionary dictionary];
        }
    }
    
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
            
            NSDictionary* jdict = [NSJSONSerialization
                                   JSONObjectWithData:[content dataUsingEncoding:NSUTF8StringEncoding]
                                   options:0
                                   error:nil];
            
            _desManifestDict = [NSMutableDictionary dictionaryWithDictionary:jdict];
        }
        
        else {
            _desManifestDict = [NSMutableDictionary dictionary];
        }
    }
}

static void saveManifest() {
    if (_srcManifestDict.count) {
        NSError *error;
        NSData *jsonData = [NSJSONSerialization
                            dataWithJSONObject:_srcManifestDict
                            options:NSJSONWritingPrettyPrinted
                            error:&error];
        if (!jsonData) {
            NSLog(@"manifest save json failed %@", error);
        }
        else {
            if (![jsonData writeToFile:_srcManifestPath atomically:YES]) {
                NSLog(@"manifest save json failed");
            }
        }
    }
    
    if (_desManifestDict.count) {
        NSError *error;
        NSData *jsonData = [NSJSONSerialization
                            dataWithJSONObject:_desManifestDict
                            options:NSJSONWritingPrettyPrinted
                            error:&error];
        if (!jsonData) {
            NSLog(@"manifest save json failed %@", error);
        }
        else {
            if (![jsonData writeToFile:_desManifestPath atomically:YES]) {
                NSLog(@"manifest save json failed");
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
    return ret;
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
    
    NSLog(@"execute: %@", cmd);
    system(cmd.UTF8String);
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

static NSMutableDictionary* getModules(NSString* folder)
{
    NSFileManager *fmgr = [NSFileManager defaultManager];
    NSError *err = nil;
    NSArray *subfiles = [fmgr subpathsOfDirectoryAtPath:folder error:&err];
    
    NSMutableDictionary* dict = [NSMutableDictionary dictionary];
    
    for (NSString* name in subfiles) {
        
        if ([name.lastPathComponent isEqualToString:@".DS_Store"]) {
            continue;
        }
        
        if ([name hasPrefix:@".git"]) {
            continue;
        }
        
        if ([name.lastPathComponent isEqualToString:_manifest_name]) {
            continue;
        }
        
        if ([name.pathExtension.lowercaseString isEqualToString:@"lua"]) {
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
    
    for (NSString* key in [dict allKeys]) {
        
        NSArray* array = dict[key];
        
        NSMutableDictionary* subdict = [NSMutableDictionary dictionary];
        
        for (NSString* name in array) {
            
            NSString* full = [folder stringByAppendingPathComponent:name];
            
            NSDictionary* attr = [fmgr attributesOfItemAtPath:full error:nil];
            
            NSDictionary* fdict = fileDict([attr[NSFileSize] longLongValue],
                                           false,
                                           fileGitTime(full),
                                           nil);
            
            [subdict setObject:fdict forKey:[[folder lastPathComponent] stringByAppendingPathComponent:name]];
        }
        
        NSDictionary* fdict = fileDict(0,
                                       true,
                                       0,
                                       subdict);
        [dict setObject:fdict forKey:key];
    }
    
    return dict;
}

static void checkSrcAssetsTimestamp() {
    
    NSDictionary* modules = getModules([_srcManifestPath stringByDeletingLastPathComponent]);
    
    NSMutableDictionary* assets = _srcManifestDict[@"assets"];
    
    NSMutableDictionary* diff = [NSMutableDictionary dictionary];
    
    for (NSString* key in [modules allKeys]) {
        NSDictionary* dict_real = modules[key];
        NSDictionary* dict_manifest = assets[key];
        
        if (!dict_manifest.count) {
            [diff setObject:dict_real forKey:key];
            continue;
        }
        
        dict_real = dict_real[@"content"];
        dict_manifest = dict_manifest[@"content"];
        
        NSMutableDictionary* subdiff = [NSMutableDictionary dictionary];
        for (NSString* path in [dict_real allKeys]) {
            NSDictionary* sub_dict_real = dict_real[path];
            NSDictionary* sub_dict_manifest = dict_manifest[path];
            
            if (sub_dict_real && sub_dict_manifest) {
                if (sub_dict_manifest[@"len"] && sub_dict_real[@"len"]
                    && [sub_dict_manifest[@"len"] isEqualToNumber:sub_dict_real[@"len"]]
                    && sub_dict_manifest[@"timestamp"] && sub_dict_real[@"timestamp"]
                    && [sub_dict_manifest[@"timestamp"] isEqualToNumber:sub_dict_real[@"timestamp"]]) {
                    continue;
                }
            }
            
            [subdiff setObject:sub_dict_real forKey:path];
        }
        
        if (subdiff.count) {
            [diff setObject:subdiff forKey:key];
        }
    }
    
    NSLog(@"files different from manifest: \n %@", diff);
    
    diff = [NSMutableDictionary dictionary];
    
    for (NSString* key in [assets allKeys]) {
        NSDictionary* dict_real = modules[key];
        NSDictionary* dict_manifest = assets[key];
        
        if (!dict_real.count) {
            [diff setObject:dict_manifest forKey:key];
            continue;
        }
        
        dict_real = dict_real[@"content"];
        dict_manifest = dict_manifest[@"content"];
        
        NSMutableDictionary* subdiff = [NSMutableDictionary dictionary];
        for (NSString* path in [dict_manifest allKeys]) {
            NSDictionary* sub_dict_real = dict_real[path];
            NSDictionary* sub_dict_manifest = dict_manifest[path];
            
            if (sub_dict_real && sub_dict_manifest) {
                if (sub_dict_manifest[@"len"] && sub_dict_real[@"len"]
                    && [sub_dict_manifest[@"len"] isEqualToNumber:sub_dict_real[@"len"]]
                    && sub_dict_manifest[@"timestamp"] && sub_dict_real[@"timestamp"]
                    && [sub_dict_manifest[@"timestamp"] isEqualToNumber:sub_dict_real[@"timestamp"]]) {
                    continue;
                }
            }
            
            [subdiff setObject:sub_dict_manifest forKey:path];
        }
        
        if (subdiff.count) {
            [diff setObject:subdiff forKey:key];
        }
    }
    
    NSLog(@"manifests different from files: \n %@", diff);
}

static void addAssets() {

    NSFileManager *fmgr = [NSFileManager defaultManager];
    NSError *err = nil;
    
    NSString* folder = [_srcManifestPath stringByDeletingLastPathComponent];
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
            continue;
        }
        
        if ([name.pathExtension.lowercaseString isEqualToString:@"lua"]) {
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
    
    NSString* upFolder = [[_srcManifestPath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
    
    for (NSString* key in [dict allKeys]) {
        
        NSArray* array = dict[key];
        
        NSMutableDictionary* subdict = [NSMutableDictionary dictionary];
        
        NSString* desPath = [[_desManifestPath stringByDeletingLastPathComponent] stringByAppendingPathComponent:key];
        
        [fmgr removeItemAtPath:desPath error:nil];
        
        NSMutableString* cmd = [NSMutableString stringWithFormat:@"cd %@ && zip %@ ", upFolder, desPath];
        
        for (NSString* name in array) {
            
            NSString* full = [folder stringByAppendingPathComponent:name];
            
            NSDictionary* attr = [fmgr attributesOfItemAtPath:full error:nil];
            
            NSDictionary* fdict = fileDict([attr[NSFileSize] longLongValue],
                                           false,
                                           fileGitTime(full),
                                           nil);
            
            [subdict setObject:fdict forKey:[[_srcFolderPath lastPathComponent] stringByAppendingPathComponent:name]];
            
            [cmd appendString:[[_srcFolderPath lastPathComponent] stringByAppendingPathComponent:name]];
            [cmd appendString:@" "];
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
    
    [_srcManifestDict setObject:dict forKey:@"assets"];
    [_desManifestDict setObject:dict forKey:@"assets"];
}

int main(int argc, const char * argv[]) {
    
    @autoreleasepool {
        
        // insert code here..
        if (argc < 2) {
            printf("\n使用 jsonmaker cksrc src-path 检测源 assets 。\n");
            printf("使用 jsonmaker reset src-path des-path 清空后重新添加 assets 。\n");
            return -1;
        }
        
        NSString* cmd = [NSString stringWithUTF8String:argv[1]];
        
        if ([cmd.lowercaseString isEqualToString:@"cksrc"]) {
            //  cksrc ~/Desktop/src/project.manifest
            _srcManifestPath = [NSString stringWithUTF8String:argv[2]];
            
            NSString* status = checkGit();
            if (status.length) {
                NSLog(@"git repository not clean:\n %@", status);
            }
            else {
                readManifest();
                checkSrcAssetsTimestamp();
            }
        }
        else if ([cmd.lowercaseString isEqualToString:@"reset"]) {
            // reset ~/Developer/work/fish-lua-clear/src ~/Desktop/src
            _srcFolderPath = [NSString stringWithUTF8String:argv[2]];
            _desFolderPath = [NSString stringWithUTF8String:argv[3]];
            
            NSString* status = checkGit();
            if (status.length) {
                NSLog(@"git repository not clean:\n %@", status);
            }
            else {
                luaCompile();
                readManifest();
                addAssets();
                saveManifest();
            }
        }
        else {
            NSLog(@"\n无效的命令: %@\n", cmd);
        }
    }
    return 0;
}
