#include "WilHelper.h"
#include <fstream>
#include "gzip/decompress.hpp"

WilHelper::WilHelper(std::string wix_filename, std::string wil_filename) :
    wix_filename_(wix_filename),
    wil_filename_(wil_filename)
{
    ;
}

void WilHelper::Load()
{
    {
        WixHeader wix_header;
        std::ifstream fin(this->wix_filename_, std::ios::in | std::ios::binary);
        fin.read(reinterpret_cast<char*>(&wix_header), sizeof(WixHeader));
        for (int i = 0; i < wix_header.index_count; i++) {
            int index = 0;
            fin.read(reinterpret_cast<char*>(&index), 4);
            this->image_indexs.push_back(index);
        }
        fin.close();
    }

    {
        WilHeader header;
        std::ifstream fin(this->wil_filename_, std::ios::in | std::ios::binary);
        fin.read(reinterpret_cast<char*>(&header), sizeof(WilHeader));

        // 调色板
        this->palette = std::make_unique<BYTE[]>(header.palette_size);
        fin.read(reinterpret_cast<char*>(this->palette.get()), header.palette_size);
        fin.close();
    }
}

bool WilHelper::CheckImage(int index)
{
    if (index < 0 || index >= this->image_indexs.size()) {
        return false;
    }
    return true;
}

std::shared_ptr<sf::Image> WilHelper::GetImage(int index)
{
    auto it = this->images.find(index);
    if (it != this->images.end()) {
        return it->second;
    }

    std::ifstream fin(this->wil_filename_, std::ios::in | std::ios::binary);
    fin.seekg(this->image_indexs[index]);

    WilImage wil_image;
    fin.read(reinterpret_cast<char*>(&wil_image), sizeof(WilImage));

    // 8bit -> argb
    int pixel_size = 4 * wil_image.width * wil_image.height;
    std::unique_ptr<BYTE[]> pixels = std::make_unique<BYTE[]>(pixel_size);
    ZeroMemory(pixels.get(), pixel_size);

    // 从下往上
    for (int y = 0; y < wil_image.height; y++) {
        for (int x = 0; x < wil_image.width; x++) {
            BYTE i_pal;
            fin.read(reinterpret_cast<char*>(&i_pal), sizeof(BYTE));
            int j = (wil_image.height - 1 - y) * wil_image.width + x;
            pixels[j * 4 + 3] = palette[i_pal * 4 + 3]; // alpha
            pixels[j * 4 + 2] = palette[i_pal * 4 + 0]; // blue
            pixels[j * 4 + 1] = palette[i_pal * 4 + 1]; // green
            pixels[j * 4 + 0] = palette[i_pal * 4 + 2]; // red
        }
    }

    fin.close();

    std::shared_ptr<sf::Image> image = std::make_shared<sf::Image>();
    image->create(wil_image.width, wil_image.height, pixels.get());
    images[index] = image;

    return image;
}

MLibrary::MLibrary(std::string filename) :
    filename_(filename)
{
    ;
}

void MLibrary::Initialize()
{
    initialized_ = true;

    std::ifstream fin(this->filename_, std::ios::in | std::ios::binary);

    int current_version;
    fin.read(reinterpret_cast<char*>(&current_version), sizeof(int));

    int count;
    fin.read(reinterpret_cast<char*>(&count), sizeof(int));

    int frameSeek = 0;
    if (current_version >= 3) {
        fin.read(reinterpret_cast<char*>(&frameSeek), sizeof(int));
    }

    for (int i = 0; i < count; i++) {
        int index = 0;
        fin.read(reinterpret_cast<char*>(&index), sizeof(int));
        this->image_indexs_.push_back(index);
    }
    if (current_version >= 3) {
        ;
    }

    fin.close();
}

std::shared_ptr<MImage> MLibrary::GetImage(int index)
{
    auto it = this->images_.find(index);
    if (it == this->images_.end()) {
        std::ifstream fin(this->filename_, std::ios::in | std::ios::binary);

        auto pos = this->image_indexs_[index];
        fin.seekg(pos);
        std::shared_ptr<MImage> it = std::make_shared<MImage>();
        fin.read(reinterpret_cast<char*>(it.get()), sizeof(MImage));

        this->images_[index] = it;

        fin.close();
    }

    return this->images_[index];
}

bool MLibrary::CheckImage(int index)
{
    if (!initialized_) {
        Initialize();
    }
    if (index < 0 || index >= this->image_indexs_.size()) {
        return false;
    }

    auto image = this->GetImage(index);
    if (image->Length <= 0) {
        return false;
    }

    return true;
}

void MLibrary::GetImageInfo(int index, int& width, int& height, int& x, int& y)
{
    if (!initialized_) {
        Initialize();
    }
    auto image = this->GetImage(index);
    width = image->Width;
    height = image->Height;
    x = image->X;
    y = image->Y;
}

std::shared_ptr<sf::Image> MLibrary::GetSfImage(int index)
{
    auto it = this->sf_images_.find(index);
    if (it == this->sf_images_.end()) {
        std::ifstream fin(this->filename_, std::ios::in | std::ios::binary);
        auto pos = this->image_indexs_[index];
        fin.seekg(pos);

        MImage image;
        ZeroMemory(&image, sizeof(MImage));
        fin.read(reinterpret_cast<char*>(&image), sizeof(MImage));

        std::unique_ptr<BYTE[]> gzip_data = std::make_unique<BYTE[]>(image.Length);
        fin.read(reinterpret_cast<char*>(gzip_data.get()), image.Length);
        fin.close();

        int pixels_len = 4 * image.Width * image.Height;
        std::unique_ptr<BYTE[]> pixels = std::make_unique<BYTE[]>(pixels_len);
        auto s = gzip::decompress(reinterpret_cast<const char*>(gzip_data.get()), image.Length);
        memcpy_s(pixels.get(), pixels_len, s.data(), pixels_len);
        for (int i = 0; i < image.Width * image.Height; i++) {
            auto temp = pixels[i * 4 + 0];
            pixels[i * 4 + 0] = pixels[i * 4 + 2];
            pixels[i * 4 + 2] = temp;
        }
        std::shared_ptr<sf::Image> sf_image = std::make_shared<sf::Image>();
        sf_image->create(image.Width, image.Height, pixels.get());

        sf_images_[index] = sf_image;
    }

    return sf_images_[index];
}
