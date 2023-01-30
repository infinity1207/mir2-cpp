#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <map>
#include <memory>
#include <Windows.h>

#pragma pack(1)
struct WixHeader
{
    char title[44];
    int index_count;
};

struct WilHeader
{
    char title[44];
    int image_count;
    int color_count;
    int palette_size;
};

struct WilImage
{
    short width;
    short height;
    short offset_x;
    short offset_y;
};

struct MImage
{
    short Width;
    short Height;
    short X;
    short Y;
    short ShadowX;
    short ShadowY;
    BYTE Shadow;
    int Length;
};
#pragma pack()

class WilHelper
{
public:
    WilHelper(std::string wix_filename, std::string wil_filename);

    void Load();
    bool CheckImage(int index);
    std::shared_ptr<sf::Image> GetImage(int index);

private:
    std::string  wix_filename_;
    std::string  wil_filename_;

    std::vector<int> image_indexs;
    std::unique_ptr<BYTE[]> palette;
    std::map<int, std::shared_ptr<sf::Image>> images;
};

class MLibrary
{
public:
    MLibrary(std::string filename);
    bool CheckImage(int index);
    void GetImageInfo(int index, int& width, int& height, int& x, int& y);
    std::shared_ptr<sf::Image> GetSfImage(int index);

private:
    void Initialize();
    std::shared_ptr<MImage> GetImage(int index);

    std::string filename_;
    bool initialized_{ false };
    std::vector<int> image_indexs_;
    std::map<int, std::shared_ptr<MImage>> images_;
    std::map<int, std::shared_ptr<sf::Image>> sf_images_;
};
