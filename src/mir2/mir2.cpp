// mir2-cpp.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "mir2.h"
#include <SFML/Graphics.hpp>
#include <fstream>
#include <vector>
#include <memory>
#include "WilHelper.h"
#include <fmt/format.h>
#include <math.h>
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#pragma pack(1)
struct MapHeader
{
    BYTE reserved0[21];
    UINT16 w;
    UINT16 xor_;
    UINT16 h;
    BYTE reserved1[27];
};

struct MapTile
{
    /** 背景图索引 */
    UINT16 bkImgIdx;
    /** 补充背景图索引 */
    UINT16 midImgIdx;
    /** 对象图索引 */
    UINT16 objImgIdx;
    /** 门索引 */
    BYTE doorIdx;
    /** 门偏移 */
    BYTE doorOffset;
    /** 动画帧数 */
    BYTE aniFrame;
    /** 动画跳帧数 */
    BYTE aniTick;
    /** 资源文件索引 */
    BYTE objFileIdx;
    /** 亮度 */
    BYTE light;
};

struct CellInfo
{
    DWORD BackImage;
    WORD MiddleImage;
    WORD FrontImage;
    BYTE DoorIndex;
    BYTE DoorOffset;
    BYTE FrontAnimationFrame;
    BYTE FrontAnimationTick;
    BYTE FrontIndex;
    BYTE Light;
    BYTE Unknow;
};
#pragma pack()

std::shared_ptr<spdlog::logger> logger;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    sf::RenderWindow window(sf::VideoMode(1920, 1080), "SFML works!");
    window.setFramerateLimit(60);

    auto wSize = window.getSize();
    sf::View view(sf::FloatRect(0, 0, wSize.x, wSize.y));
    window.setView(view);

    logger = spdlog::rotating_logger_mt("file_logger", "d:\\mir2.log", 1048576 * 10, 9);
    logger->set_pattern("%Y-%m-%d %T,%f - [%t] - %l - %v");

    int CellWidth = 48;
    int CellHeight = 32;
    int AnimationCount = 0;

    // 加载地图
    MapHeader header;
    std::vector< std::vector<std::shared_ptr<CellInfo>> > tiles;
    {
        std::ifstream fin("d:\\work\\vs2017\\mir2\\Build\\Client\\Map\\0.map", std::ios::in | std::ios::binary);

        int width = 0;
        int height = 0;
        fin.read(reinterpret_cast<char*>(&header), sizeof(MapHeader));
        width = header.w ^ header.xor_;
        height = header.h ^ header.xor_;
        for (int x = 0; x < width; x++) {
            std::vector<std::shared_ptr<CellInfo>> v;
            for (int y = 0; y < height; y++) {
                std::shared_ptr<CellInfo> cell = std::make_shared<CellInfo>();
                ZeroMemory(cell.get(), sizeof(CellInfo));
                fin.read(reinterpret_cast<char*>(cell.get()), sizeof(CellInfo));
                v.push_back(cell);
            }
            tiles.push_back(v);
        }
        fin.close();
    }

    std::map<int, std::shared_ptr<MLibrary>> libs;
    libs[0] = std::make_shared<MLibrary>("d:\\work\\vs2017\\mir2\\Build\\Client\\Data\\Map\\WemadeMir2\\Tiles.Lib");
    libs[1] = std::make_shared<MLibrary>("d:\\work\\vs2017\\mir2\\Build\\Client\\Data\\Map\\WemadeMir2\\Smtiles.Lib");
    libs[2] = std::make_shared<MLibrary>("d:\\work\\vs2017\\mir2\\Build\\Client\\Data\\Map\\WemadeMir2\\Objects.Lib");
    for (int i = 2; i < 27; i++) {
        auto filename = fmt::format("d:\\work\\vs2017\\mir2\\Build\\Client\\Data\\Map\\WemadeMir2\\Objects{0}.Lib", i);
        libs[i + 1] = std::make_shared<MLibrary>(filename);
    }

    int step = 2;

    int movment_y = 296;
    int movment_x = 294;
    int range_w = 40;
    int range_h = 50;
    int drawY, drawX;

    std::vector< std::vector<std::shared_ptr<sf::Texture>> > back_textures;
    for (int x = 0; x < range_w; x++) {
        if (x % 2 == 1) {
            continue;
        }
        std::vector<std::shared_ptr<sf::Texture>> col;
        for (int y = 0; y < range_h; y++) {
            if (y % 2 == 1) {
                continue;
            }
            std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();
            texture->create(CellWidth * 2, CellHeight * 2);
            col.push_back(texture);
        }
        back_textures.push_back(col);
    }

    sf::Clock clock;
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Left) {
                    movment_x -= step;
                    movment_x = std::max(movment_x, 0);
                }
                else if (event.key.code == sf::Keyboard::Right) {
                    movment_x += step;
                    movment_x = std::min(movment_x, 600);
                }
                else if (event.key.code == sf::Keyboard::Up) {
                    movment_y -= step;
                    movment_y = std::max(movment_y, 0);
                }
                else if (event.key.code == sf::Keyboard::Down) {
                    movment_y += step;
                    movment_y = std::min(movment_y, 600);
                }
            }
        }

        window.clear();

        logger->info("==== 开始绘制 ====");
        clock.restart();
        for (int y = movment_y; y < movment_y + range_h; y++) {
            if (y % 2 == 1) {
                continue;
            }
            drawY = (y - movment_y) * 32;
            for (int x = movment_x; x < movment_x + range_w; x++) {
                if (x % 2 == 1) {
                    continue;
                }
                drawX = (x - movment_x) * 48;
                int img_idx = ((tiles[x][y]->BackImage ^ 0xAA38AA38) & 0x1FFFFFFF) - 1;
                auto lib = libs[0];
                if (!lib->CheckImage(img_idx)) {
                    continue;
                }

                int width, height, offset_x, offset_y;
                lib->GetImageInfo(img_idx, width, height, offset_x, offset_y);
                auto sf_image = lib->GetSfImage(img_idx);

                auto texture = back_textures[(x - movment_x) / 2][(y - movment_y) / 2];
                texture->update(*(sf_image.get()));

                sf::RectangleShape rect_shape(sf::Vector2f(static_cast<float>(width), static_cast<float>(height)));
                rect_shape.setPosition(sf::Vector2f(static_cast<float>(drawX), static_cast<float>(drawY)));
                rect_shape.setTexture(texture.get());
                window.draw(rect_shape);
            }
        }
        sf::Time elapsed1 = clock.getElapsedTime();
        logger->info("绘制地图背景层耗时: {0}微秒", elapsed1.asMicroseconds());

        clock.restart();
        for (int y = movment_y; y < movment_y + range_h; y++) {
            drawY = (y - movment_y) * CellHeight;
            for (int x = movment_x; x < movment_x + range_w; x++) {
                drawX = (x - movment_x) * CellWidth;
                auto lib = libs[1];
                int img_idx = (tiles[x][y]->MiddleImage ^ header.xor_) - 1;
                if (!lib->CheckImage(img_idx)) {
                    continue;
                }

                int width, height, offset_x, offset_y;
                lib->GetImageInfo(img_idx, width, height, offset_x, offset_y);
                auto image = lib->GetSfImage(img_idx);
                sf::Texture texture;
                texture.loadFromImage(*(image.get()));

                sf::RectangleShape rect_shape(sf::Vector2f(static_cast<float>(width), static_cast<float>(height)));
                rect_shape.setPosition(static_cast<float>(drawX), static_cast<float>(drawY));
                rect_shape.setTexture(&texture);

                window.draw(rect_shape);
            }
        }
        sf::Time elapsed2 = clock.getElapsedTime();
        logger->info("绘制地图中间层耗时: {0}微秒", elapsed2.asMicroseconds());

        // Draw Font
        clock.restart();
        for (int y = movment_y; y < movment_y + range_h; y++) {
            drawY = (y - movment_y) * CellHeight;
            for (int x = movment_x; x < movment_x + range_w; x++) {
                drawX = (x - movment_x) * CellWidth;

                int fileIndex = tiles[x][y]->FrontIndex + 2;
                if (fileIndex == -1) {
                    continue;
                }

                auto lib = libs[fileIndex];
                int index = ((tiles[x][y]->FrontImage ^ header.xor_) & 0x7FFF) - 1;
                if (!lib->CheckImage(index)) {
                    continue;
                }
                int width, height, offset_x, offset_y;
                lib->GetImageInfo(index, width, height, offset_x, offset_y);
                if ((width != CellWidth || height != CellHeight) && ((width != CellWidth * 2) || (height != CellHeight * 2))) {
                    continue;
                }

                auto image = lib->GetSfImage(index);
                sf::Texture texture;
                texture.loadFromImage(*(image.get()));

                sf::RectangleShape rect_shape(sf::Vector2f(static_cast<float>(width), static_cast<float>(height)));
                sf::Vector2f point(static_cast<float>(drawX), static_cast<float>(drawY));
                rect_shape.setPosition(point);
                rect_shape.setTexture(&texture);

                window.draw(rect_shape);
            }
        }
        sf::Time elapsed3 = clock.getElapsedTime();
        logger->info("绘制地图前景层耗时: {0}微秒", elapsed3.asMicroseconds());

        // Draw Objects
        clock.restart();
        for (int y = movment_y; y < movment_y + range_h; y++) {
            drawY = (y - movment_y) * CellHeight;
            for (int x = movment_x; x < movment_x + range_w; x++) {
                drawX = (x - movment_x) * CellWidth;

                BYTE animation;
                bool blend;
                int fileIndex = tiles[x][y]->FrontIndex + 2;
                if (fileIndex == -1) {
                    continue;
                }

                animation = tiles[x][y]->FrontAnimationFrame;

                if ((animation & 0x80) > 0)
                {
                    blend = true;
                    animation &= 0x7F;
                }
                else {
                    blend = false;
                }

                int index = ((tiles[x][y]->FrontImage ^ header.xor_) & 0x7FFF) - 1;
                if (animation) {
                    BYTE animationTick = tiles[x][y]->FrontAnimationTick;
                    index += (AnimationCount % (animation + (animation * animationTick))) / (1 + animationTick);
                }

                auto lib = libs[fileIndex];
                if (!lib->CheckImage(index)) {
                    continue;
                }

                int width, height, offset_x, offset_y;
                lib->GetImageInfo(index, width, height, offset_x, offset_y);

                if (width == CellWidth && height == CellHeight && animation == 0) {
                    continue;
                }
                if ((width == CellWidth * 2) && (height == CellHeight * 2) && (animation == 0)) {
                    continue;
                }

                auto image = lib->GetSfImage(index);
                sf::Texture texture;
                texture.loadFromImage(*(image.get()));
                sf::RectangleShape rect(sf::Vector2f(static_cast<float>(width), static_cast<float>(height)));
                sf::Vector2f point(static_cast<float>(drawX), static_cast<float>(drawY));
                sf::RenderStates states;

                if (blend) {
                    if ((fileIndex > 99) & (fileIndex < 199)) {
                        ;
                    }
                    else {
                        states.blendMode = sf::BlendAdd;
                        if (index >= 2723 && index <= 2732) {
                            point.x = point.x + offset_x;
                            point.y = point.y - height + offset_y + CellHeight;
                        }
                    }
                }
                else {
                    point.y = point.y - height + CellHeight;
                }
                rect.setPosition(point);
                rect.setTexture(&texture);
                window.draw(rect, states);
            }
        }
        sf::Time elapsed4 = clock.getElapsedTime();
        logger->info("绘制地图物体耗时: {0}微秒", elapsed4.asMicroseconds());

        logger->info("==== 绘制结束 ====\n");

        AnimationCount++;

        window.display();
    }

    return 0;
}
