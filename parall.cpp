// блюрим изображения с параллелизацией

#include <iostream>
#include <vector>
#include <queue>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

template <typename T>

class BlockingQueue { // блокирующая очередь
private: 
    queue<T> queue_;
    mutable mutex mtx_;
    condition_variable cv_;

public:
    void push(const T& item) {
        {
            lock_guard<mutex> lock(mtx_);
            queue_.push(item);
        }
        cv_.notify_one();
    }

    T pop() {
        unique_lock<mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T item = queue_.front();
        queue_.pop();
        return item;
    }

    bool empty() const {
        lock_guard<mutex> lock(mtx_);
        return queue_.empty();
    }
};

// загружаем изображения и кидаем в inputQueue 
void producer(BlockingQueue<pair<uint8_t*, tuple<int, int, int>>>& inputQueue, const vector<string>& imagePaths) {
    for (const auto& path : imagePaths) {
        int width, height, channels;
        uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (data) {
            inputQueue.push({data, {width, height, channels}});
        } else {
            cerr << "Failed to load image: " << path << endl;
        }
    }
}

// берет из inputQueue изображения, блюрит и помещает в outputQueue
void consumer(BlockingQueue<pair<uint8_t*, tuple<int, int, int>>>& inputQueue,
              BlockingQueue<pair<uint8_t*, tuple<int, int, int>>>& outputQueue, 
              atomic<bool>& flag) {
    while (!flag || !inputQueue.empty()) {
        if (!inputQueue.empty()) {
            auto [data, dims] = inputQueue.pop();
            auto [width, height, channels] = dims;

            // размытие
            uint8_t* blurred = new uint8_t[width * height * channels];
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    for (int c = 0; c < channels; ++c) {
                        int sum = 0, count = 0;
                        for (int dy = -2; dy <= 2; ++dy) {
                            for (int dx = -2; dx <= 2; ++dx) {
                                int nx = x + dx;
                                int ny = y + dy;
                                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                    sum += data[(ny * width + nx) * channels + c];
                                    count++;
                                }
                            }
                        }
                        blurred[(y * width + x) * channels + c] = sum / count;
                    }
                }
            }

            outputQueue.push({blurred, {width, height, channels}});

            stbi_image_free(data);
        }
    }
}

int main() {
    vector<string> inputPaths;
    for (int i = 1; i <= 20; ++i) {
        inputPaths.push_back("images/image" + to_string(i) + ".jpg");
    }

    // очереди
    BlockingQueue<pair<uint8_t*, tuple<int, int, int>>> inputQueue;
    BlockingQueue<pair<uint8_t*, tuple<int, int, int>>> outputQueue;

    // флаг завершения работы 
    atomic<bool> flag(false);

    // время начала
    auto startTime = chrono::high_resolution_clock::now();

    // вот тут хихи, вообще не придумала как по-другому
    thread producerThread(producer, ref(inputQueue), ref(inputPaths));

    vector<thread> consumerThreads;
    int numConsumers = 4; 
    for (int i = 0; i < numConsumers; ++i) {
        consumerThreads.emplace_back(consumer, ref(inputQueue), ref(outputQueue), ref(flag));
    }

    // ждем завершения работы производителя
    producerThread.join();
    flag = true;

    // ждем завершения работы всех потребителей
    for (auto& t : consumerThreads) {
        t.join();
    }

    // время окончания
    auto endTime = chrono::high_resolution_clock::now();

    // сохраняем
    int i = 1;
    while (!outputQueue.empty()) {
        auto [data, dims] = outputQueue.pop();
        auto [width, height, channels] = dims;

        string outputPath = "res/blurred_image" + to_string(i++) + ".jpg";
        if (!stbi_write_jpg(outputPath.c_str(), width, height, channels, data, 90)) {
            cerr << "Failed to save image: " << outputPath << endl;
        } else {
            cout << "Saved: " << outputPath << endl;
        }

        delete[] data;
    }

    // auto endTime = chrono::high_resolution_clock::now();

    chrono::duration<double> elapsedTime = endTime - startTime;

    cout << "Processing completed in " << elapsedTime.count() << " seconds!" << endl;    
    return 0;
}
