//  без параллелизации, для сравнения времени обработки

#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

// размытие
void applyBlur(const uint8_t* inputImage, uint8_t* outputImage, int width, int height, int channels) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                int sum = 0, count = 0;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            int idx = (ny * width + nx) * channels + c;
                            sum += inputImage[idx];
                            count++;
                        }
                    }
                }
                int idx = (y * width + x) * channels + c;
                outputImage[idx] = static_cast<uint8_t>(sum / count);
            }
        }
    }
}

int main() {
    vector<string> inputPaths;
    for (int i = 1; i <= 20; ++i) {
        inputPaths.push_back("images/image" + to_string(i) + ".jpg");
    }

    vector<uint8_t*> inputImages;
    vector<int> widths, heights, channelsList;

    // загружаем изображения
    for (const auto& path : inputPaths) {
        int width, height, channels;
        uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (data) {
            inputImages.push_back(data);
            widths.push_back(width);
            heights.push_back(height);
            channelsList.push_back(channels);
        } else {
            cerr << "Failed to load image: " << path << endl;
        }
    }

    if (inputImages.empty()) {
        cerr << "No images loaded!" << endl;
        return 1;
    }

    vector<uint8_t*> outputImages(inputImages.size());

    for (size_t i = 0; i < inputImages.size(); ++i) {
        outputImages[i] = new uint8_t[widths[i] * heights[i] * channelsList[i]];
    }

    auto startTime = chrono::high_resolution_clock::now();

    // блюрим
    for (size_t i = 0; i < inputImages.size(); ++i) {
        applyBlur(inputImages[i], outputImages[i], widths[i], heights[i], channelsList[i]);
    }

    auto endTime = chrono::high_resolution_clock::now();
   

    // сохраняем
    for (size_t i = 0; i < outputImages.size(); ++i) {
        string outputPath = "res/blurred_image" + to_string(i + 1) + ".jpg";
        if (!stbi_write_jpg(outputPath.c_str(), widths[i], heights[i], channelsList[i], outputImages[i], 90)) {
            cerr << "Failed to save image: " << outputPath << endl;
        } else {
            cout << "Saved: " << outputPath << endl;
        }

        stbi_image_free(inputImages[i]);
        delete[] outputImages[i];
    }

    // auto endTime = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsedTime = endTime - startTime;

    cout << "Processing completed in " << elapsedTime.count() << " seconds!" << endl;
    return 0;
}
