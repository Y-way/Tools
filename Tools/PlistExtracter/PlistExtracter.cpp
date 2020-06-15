#
# Copyright (c) 2020 Y-way
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 

#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/Image.h>
#include <Urho3D/Resource/PListFile.h>
#include <Urho3D/DebugNew.h>
#include <Urho3D/Urho2D/Sprite2D.h>
#include <Urho3D/Urho2D/SpriteSheet2D.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Resource/XMLElement.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/Container/RefCounted.h>
#ifdef WIN32
#include <windows.h>
#endif

#define STBRP_LARGE_RECTS
#define STB_RECT_PACK_IMPLEMENTATION
#include <STB/stb_rect_pack.h>

using namespace Urho3D;

int main(int argc, char** argv);
void Run(Vector<String>& arguments);

void Help()
{
    ErrorExit("Usage: PlistExtracter -options \n"
        "\n"
        "Options:\n"
        "-h Shows this help message.\n"
        "-plist Plist file name.\n");
}

int main(int argc, char** argv)
{
    Vector<String> arguments;

#ifdef WIN32
    arguments = ParseArguments(GetCommandLineW());
#else
    arguments = ParseArguments(argc, argv);
#endif

    Run(arguments);
    return 0;
}

struct FrameInfo {
    IntRect rect;
    IntVector2 offset;
};

bool ParserPListFile(SharedPtr<PListFile> plist, HashMap<String, FrameInfo>& out)
{
    const PListValueMap& root = plist->GetRoot();
    const PListValueMap& frames = root["frames"]->GetValueMap();
    for (PListValueMap::ConstIterator i = frames.Begin(); i != frames.End(); ++i)
    {
        String name = i->first_;

        const PListValueMap& frameInfo = i->second_.GetValueMap();
        
        bool ratate = frameInfo["rotated"]->GetBool();
        IntRect rectangle = frameInfo["frame"]->GetIntRect(ratate);
        if (ratate)
        {
            URHO3D_LOGWARNING(name + " is ratated!");
        }

        Vector2 hotSpot(0.5f, 0.5f);
        IntVector2 offset(0, 0);
        PListValueMap::ConstIterator it = frameInfo.Find("offset");
        if (it != frameInfo.End())
        {
            offset = it->second_.GetIntVector2();
        }

        IntRect sourceColorRect = frameInfo["sourceColorRect"]->GetIntRect();
        if (sourceColorRect.left_ != 0 && sourceColorRect.top_ != 0)
        {
            offset.x_ = -sourceColorRect.left_;
            offset.y_ = -sourceColorRect.top_;

            IntVector2 sourceSize = frameInfo["sourceSize"]->GetIntVector2();
            hotSpot.x_ = (offset.x_ + sourceSize.x_ / 2.f) / rectangle.Width();
            hotSpot.y_ = 1.0f - (offset.y_ + sourceSize.y_ / 2.f) / rectangle.Height();
        }
        FrameInfo info;
        info.rect = rectangle;
        info.offset = offset;
        out[name] = info;
    }
    return true;
}

void Run(Vector<String>& arguments)
{
    if (arguments.Size() < 2)
        Help();

    SharedPtr<Context> context(new Context());
    context->RegisterSubsystem(new FileSystem(context));
    context->RegisterSubsystem(new Log(context));
    auto* fileSystem = context->GetSubsystem<FileSystem>();
    auto* cache = context->GetSubsystem<ResourceCache>();

    String inputfile;
    String outDir;
    bool help = false;

    while (arguments.Size() > 0)
    {
        String arg = arguments[0];
        arguments.Erase(0);

        if (arg.Empty())
            continue;

        if (arg.StartsWith("-"))
        {
            if (arg == "-plist")
            { 
                inputfile = arguments[0];
                arguments.Erase(0);
            }
            else if (arg == "-h") 
            { 
                help = true; break; 
            }
        }
    }

    if (help)
        Help();

    if (inputfile.Empty())
        ErrorExit("An input plist-file must be specified.");
    
    if (!::IsAbsolutePath(inputfile))
    {
        inputfile = fileSystem->GetProgramDir() + inputfile;
    }

    if (outDir.Empty())
    {
        String filePath;
        String fileName;
        String ext;
        ::SplitPath(inputfile, filePath, fileName, ext);
        if (outDir.Empty())
        {
            outDir = filePath + fileName;
            if (outDir.Back() != '/')
            {
                outDir += '/';
            }
        }
    }

    if (!::IsAbsolutePath(outDir))
    {
        outDir = fileSystem->GetProgramDir() + outDir;
    }
    if (!fileSystem->DirExists(outDir))
    {
        fileSystem->CreateDir(outDir);
    }

    File file(context, inputfile);
    SharedPtr<PListFile> loadPListFile_ = SharedPtr<PListFile>(new PListFile(context));
    if (!loadPListFile_->Load(file))
    {
        URHO3D_LOGERROR("Could not load sprite sheet");
        loadPListFile_.Reset();
        return;
    }
    HashMap<String, FrameInfo> sprites;
    ParserPListFile(loadPListFile_, sprites);

    const PListValueMap& root = loadPListFile_->GetRoot();
    const PListValueMap& metadata = root["metadata"]->GetValueMap();
    const String& textureFileName = metadata["realTextureFileName"]->GetString();

    // If we're async loading, request the texture now. Finish during EndLoad().
    String loadTextureName_ = GetParentPath(file.GetName()) + textureFileName;
    File imageFile(context, loadTextureName_);
    SharedPtr<Image> image = SharedPtr<Image>(new Image(context));;

    if (!image->Load(imageFile))
    {
        URHO3D_LOGERROR("Could not load sprite sheet");
        image.Reset();
        return;
    }

    String filePath;
    for (auto& var : sprites)
    {
        SharedPtr<Image> subImage(image->GetSubimage(var.second_.rect));
        if (subImage)
        {
            inputfile = outDir + var.first_;
            filePath = GetPath(inputfile);
            if (!fileSystem->DirExists(filePath))
            {
                fileSystem->CreateDir(filePath);
            }
            subImage->SavePNG(inputfile);
        }
    }
}
