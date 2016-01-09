/****************************************************************************
 Copyright (c) 2013 cocos2d-x.org
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifndef __ModuleManifest__
#define __ModuleManifest__

#include "extensions/ExtensionMacros.h"
#include "extensions/ExtensionExport.h"
#include "ModuleDownloader.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "json/document.h"

NS_CC_EXT_BEGIN


class CC_EX_DLL ModuleManifest : public Ref
{
public:
    
    friend class AssetsManagerEx;
    
    //! The type of difference
    enum class DiffType {
        ADDED,
        DELETED,
        MODIFIED
    };
    
    enum class DownloadState {
        UNSTARTED,
        DOWNLOADING,
        SUCCESSED
    };
    
    //! Asset object
    class SubAsset {
    public:
        int64_t len;
        int64_t timestamp;
        std::string path;
    };
    
    class Asset {
    public:
        int64_t len;
        int64_t timestamp;
        std::string path;
        bool compressed;
        DownloadState downloadState;
        std::unordered_map<std::string, SubAsset> content;
    };
    
    //! Object indicate the difference between two Assets
    struct AssetDiff {
        Asset asset;
        DiffType type;
    };
    
    /** @brief Check whether the ModuleManifest have been fully loaded
     */
    bool isLoaded() const;
    
    bool isForceUpdate() const;
    
    int64_t diffLength() const;
    
    void setDiffLength(int64_t len);
    
    /** @brief Gets remote package url.
     */
    const std::string& getPackageUrl() const;
    
    /** @brief Gets remote ModuleManifest file url.
     */
    const std::string& getModuleManifestFileUrl() const;
    
    /** @brief Constructor for ModuleManifest class
     @param ModuleManifestUrl Url of the local ModuleManifest
     */
    ModuleManifest(const std::string& ModuleManifestUrl = "");
    
    /** @brief Parse the ModuleManifest file information into this ModuleManifest
     * @param ModuleManifestUrl Url of the local ModuleManifest
     */
    void parse(const std::string& ModuleManifestUrl);
    
    /** @brief Check whether the version of this ModuleManifest equals to another.
     * @param b   The other ModuleManifest
     */
    bool versionEquals(const ModuleManifest *b) const;
    
    /** @brief Generate difference between this ModuleManifest and another.
     * @param b   The other ModuleManifest
     */
    std::unordered_map<std::string, AssetDiff> genDiff();
    
    /** @brief Generate resuming download assets list
     * @param units   The download units reference to be modified by the generation result
     */
    void genResumeAssetsList(ModuleDownloader::DownloadUnits *units) const;
    
    /** @brief Prepend all search paths to the FileUtils.
     */
    void prependSearchPaths();
    
    void loadModuleManifest(const rapidjson::Document &json);
    
    void saveToFile(const std::string &filepath);
    
    Asset parseAsset(const std::string &path, const rapidjson::Value &json);
    
    void clear();
    
    /** @brief Gets assets.
     */
    const std::unordered_map<std::string, Asset>& getAssets() const;
    
    /** @brief Set the download state for an asset
     * @param key   Key of the asset to set
     * @param state The current download state of the asset
     */
    void setAssetDownloadState(const std::string &key, const DownloadState &state);
    
    // tools
    static int64_t getFileLen(const std::string& filePath);
    static int64_t getFileTime(const std::string& filePath);
    static int64_t setFileTime(const std::string& filePath, long long timestamp);
    static int64_t dosDateToTime(unsigned long ulDosDate);
        
private:
    
    //! Indicate whether the ModuleManifest have been fully loaded
    bool _loaded;
    
    //! Reference to the global file utils
    FileUtils *_fileUtils;
    
    //! The local ModuleManifest root
    std::string _ModuleManifestRoot;
    
    
    bool _forceUpdate;
    
    int64_t _diffLen;
    
    //! The remote package url
    std::string _packageUrl;
    
    //! The remote path of ModuleManifest file
    std::string _remoteModuleManifestUrl;
    
    //! Full assets list
    std::unordered_map<std::string, Asset> _assets;
    
    //! All search paths
    std::vector<std::string> _searchPaths;
    
    rapidjson::Document _json;
};

NS_CC_EXT_END
#endif /* defined(__ModuleManifest__) */
