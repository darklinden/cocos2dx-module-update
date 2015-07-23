//
//  main.m
//  jsonmaker
//
//  Created by HanShaokun on 23/7/15.
//  Copyright (c) 2015 by. All rights reserved.
//

#import <Foundation/Foundation.h>

static __strong NSMutableDictionary *_manifestDict = nil;
static __strong NSString* _manifestPath = nil;

static __strong NSString* _packageUrl = nil;
static __strong NSString* _remoteManifestUrl = nil;

static void readManifest()
{
    NSString* content = [NSString stringWithContentsOfFile:_manifestPath encoding:NSUTF8StringEncoding error:nil];
    NSDictionary* jdict = [NSJSONSerialization JSONObjectWithData:[content dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
    
    _manifestDict = [NSMutableDictionary dictionaryWithDictionary:jdict];
    _packageUrl = _manifestDict[@"packageUrl"];
    _remoteManifestUrl = _manifestDict[@"remoteManifestUrl"];
}

static void saveManifest() {
    NSError *error;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:_manifestDict
                                                       options:NSJSONWritingPrettyPrinted
                                                         error:&error];
    if (! jsonData) {
        NSLog(@"manifest save json failed %@", error);
        return;
    }
    
    if (![jsonData writeToFile:_manifestPath atomically:YES]) {
        NSLog(@"manifest save json failed");
        return;
    }
}

static bool addAssets(NSString* assetsModulePath) {
    
    NSFileManager *fmgr = [NSFileManager defaultManager];
    NSError *err = nil;
    
    NSString* desPath = [[[_manifestPath stringByDeletingLastPathComponent] stringByAppendingPathComponent:assetsModulePath.lastPathComponent] stringByAppendingPathExtension:@"zip"];
    NSString *tmpPath = [[assetsModulePath stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"tmp"];
    NSString *tmpSrc = [tmpPath stringByAppendingPathComponent:@"src"];
    
    [fmgr removeItemAtPath:tmpPath error:nil];
    
    if (![fmgr createDirectoryAtPath:tmpSrc withIntermediateDirectories:YES attributes:nil error:nil]) {
        NSLog(@"create error with folder: %@\n", tmpSrc);
        return false;
    }
    
    [fmgr copyItemAtPath:assetsModulePath toPath:[tmpSrc stringByAppendingPathComponent:assetsModulePath.lastPathComponent] error:nil];
    
    NSArray *array = [fmgr subpathsOfDirectoryAtPath:tmpPath error:&err];
    
    if (err) {
        NSLog(@"error with enum folder: %@ err: %@ \n", tmpPath, err);
        return false;
    }
    
    NSMutableDictionary* dict = [NSMutableDictionary dictionary];
    NSMutableString* cmd = [NSMutableString stringWithFormat:@"cd %@ && zip %@ ", tmpPath, desPath];
    
    for (NSString *name in array) {
        
        NSLog(@"enum file: %@\n", name);
        
        if ([name.lastPathComponent isEqualToString:@".DS_Store"]) {
            continue;
        }
        
        NSString* full = [tmpPath stringByAppendingPathComponent:name];
        
        BOOL isDirectory = NO;
        BOOL exists = [fmgr fileExistsAtPath:full
                                 isDirectory:&isDirectory];
        
        if (exists && !isDirectory) {
            NSMutableDictionary* subdict = [NSMutableDictionary dictionary];
            NSDictionary* arr = [fmgr attributesOfItemAtPath:full error:nil];
            
            [subdict setObject:arr[NSFileSize] forKey:@"len"];
            
            [dict setObject:subdict forKey:name];
            
            [cmd appendString:name];
            [cmd appendString:@" "];
        }
    }
    
    [fmgr removeItemAtPath:desPath error:nil];
    system(cmd.UTF8String);
    
    if (![fmgr fileExistsAtPath:desPath]) {
        NSLog(@"zip error with folder: %@\n", assetsModulePath);
        return false;
    }
    
    NSMutableDictionary* mdict = [NSMutableDictionary dictionary];
    
    NSDictionary* arr = [fmgr attributesOfItemAtPath:desPath error:nil];
    [mdict setObject:arr[NSFileSize] forKey:@"len"];
    
    [mdict setObject:[NSNumber numberWithBool:true] forKey:@"compressed"];
    
    [mdict setObject:dict forKey:@"content"];
    
    NSMutableDictionary* assetsDict = [NSMutableDictionary dictionary];
    
    [assetsDict setObject:mdict forKey:desPath.lastPathComponent];
    
    [fmgr removeItemAtPath:tmpPath error:nil];
    
    //
    for (NSString * key in assetsDict.allKeys) {
        NSMutableDictionary* dictAsset = [NSMutableDictionary dictionaryWithDictionary:_manifestDict[@"assets"]];
        if (_manifestDict[@"assets"]) {
            dictAsset = [NSMutableDictionary dictionaryWithDictionary:_manifestDict[@"assets"]];
        }
        else {
            dictAsset = [NSMutableDictionary dictionary];
        }
        [dictAsset setObject:assetsDict[key] forKey:key];
        [_manifestDict setObject:dictAsset forKey:@"assets"];
    }
    
    return true;
}

void cleanDesPath(NSString* folder)
{
    NSArray* contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:folder error:nil];
    for (NSString* name in contents) {
        if (![name.pathExtension.lowercaseString isEqualToString:@"zip"]) {
            continue;
        }
        
        NSString* full = [folder stringByAppendingPathComponent:name];
        [[NSFileManager defaultManager] removeItemAtPath:full error:nil];
    }
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here..
        if (argc < 2) {
            printf("\n使用 jmconsole -add manifest-path assets-1 assets-2 ... 添加assets 。\n");
            printf("使用 jmconsole -clean manifest-path 清空 assets 。\n");
            printf("使用 jmconsole -reset manifest-path assets-1 assets-2 ... 清空后重新添加 assets 。\n");
            return -1;
        }
        
        NSString* cmd = [NSString stringWithUTF8String:argv[1]];
        
        _manifestPath = [NSString stringWithUTF8String:argv[2]];
        
        if (![_manifestPath.lastPathComponent.lowercaseString isEqualToString:@"project.manifest"]
            || ![[NSFileManager defaultManager] fileExistsAtPath:_manifestPath]) {
            printf("\n使用 jmconsole -add manifest-path assets-1 assets-2 ... 添加assets 。\n");
            printf("使用 jmconsole -clean manifest-path 清空 assets 。\n");
            printf("使用 jmconsole -reset manifest-path assets-1 assets-2 ... 清空后重新添加 assets 。\n");
            return -1;
        }
        
        cleanDesPath(_manifestPath.stringByDeletingLastPathComponent);
        
        readManifest();
        
        if ([cmd.lowercaseString isEqualToString:@"-clean"]) {
            [_manifestDict removeObjectForKey:@"assets"];
            saveManifest();
        }
        else if ([cmd.lowercaseString isEqualToString:@"-reset"]) {
            [_manifestDict removeObjectForKey:@"assets"];
            
            for (int i = 3; i < argc; i++) {
                NSString *assetsModulePath = [NSString stringWithUTF8String:argv[i]];
                bool success = addAssets(assetsModulePath);
                if (!success) {
                    return -1;
                }
            }
            saveManifest();
        }
        else if ([cmd.lowercaseString isEqualToString:@"-add"]) {
            for (int i = 3; i < argc; i++) {
                NSString *assetsModulePath = [NSString stringWithUTF8String:argv[i]];
                bool success = addAssets(assetsModulePath);
                if (!success) {
                    return -1;
                }
            }
            saveManifest();
        }
        else {
            printf("\n未知的命令: %s\n", cmd.UTF8String);
            printf("\n使用 jmconsole -add manifest-path assets-1 assets-2 ... 添加assets 。\n");
            printf("使用 jmconsole -clean manifest-path 清空 assets 。\n");
            printf("使用 jmconsole -reset manifest-path assets-1 assets-2 ... 清空后重新添加 assets 。\n");
        }
    }
    return 0;
}
