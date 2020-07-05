
#include <cstdint>
#include <vector>
#include <cmath>

#include "App/Input.hpp"
#include "App/Render.hpp"
#include "App/utils.hpp"
#include "debug.hpp"

Render::Render() : map(1000), camera(
        206.88, // x
        192.58, // y
        180.96, // height
        M_PI/2,  // angle
        160.58, // horizon
        350     // distance
    ) {
    width = 0;
    height = 0;
    
    buffer[0] = 0;
    buffer[1] = 0;
    buffer[2] = 0;


    renderBuffer = 0;
    lastBuffer = 1;
    readyBuffer = 2;

    lastFrame = std::chrono::steady_clock::now();
    keepRender = true;

    frameCount = 0;
    lastRender = -1;
}

Render::~Render() {
    keepRender = false;
    renderThread.join();
}

void Render::startThread() {
    map.startThreads();
    renderThread = std::thread(&Render::renderLoop, &*this);
}

void Render::updateCanvas(uint32_t cWidth, uint32_t cHeight) {
  width = cWidth;
  height = cHeight;
  uint32_t pixelCount = width * height;
  for (int i = 0; i < 3; i++) {
      if (buffer[i] != 0) {
          delete[] buffer[i];
      }
      buffer[i] = new uint32_t[pixelCount];
  }
}

void Render::renderSky(uint32_t* data, int width, int height) {
    uint32_t skyColor = rgba(135, 206, 235, 255);
    for (uint32_t i=0; i<width*height; i++) {
        data[i] = skyColor;
    }
}

uint32_t* Render::finishRender() {
    std::unique_lock<std::shared_mutex> lock(buffer_mutex_);
    int nextBuffer = lastBuffer;
    lastBuffer = renderBuffer;
    renderBuffer = nextBuffer;
    frameCount++;
    return buffer[renderBuffer];
}

uint32_t* Render::getRenderedFrame() {
    std::unique_lock<std::shared_mutex> lock(buffer_mutex_);
    if (frameCount == lastRender) {
        /* we haven't rendered a new frame since the last render, just output the last known frame to work */
        return buffer[readyBuffer];
    }
    lastRender = frameCount;
    int nextBuffer = lastBuffer;
    lastBuffer = readyBuffer;
    readyBuffer = nextBuffer;
    return buffer[readyBuffer];
}

void Render::drawVLine(uint32_t* data, uint32_t x, uint32_t ytop, uint32_t ybottom, uint32_t color) {
    if (ytop < 0) ytop = 0;
    if (ytop > ybottom) return;

    uint32_t offset = ytop * width + x;
    for (uint32_t k=ytop; k<ybottom; k++) {
        data[offset] = color;
        offset += width;
    }
}

void Render::updateCamera(std::chrono::time_point<std::chrono::steady_clock>& currentFrame) {
    const std::chrono::duration<double, std::milli> deltaFrames = currentFrame - lastFrame;

    int leftright = input.left - input.right;
    int forwardbackward = 3 * (input.up - input.down);
    int updown = 2 * (input.lookup - input.lookdown);
    double deltaTime =  0.03 * 1.2 * deltaFrames.count();
    if (leftright != 0) {
        camera.angle += leftright * 0.1 * deltaTime;
    }
    if (forwardbackward != 0) {
        camera.x -= forwardbackward * sin(camera.angle) * deltaTime;
        camera.y -= forwardbackward * cos(camera.angle) * deltaTime;
    }
    if (updown != 0) {
      camera.height += updown * deltaTime;
    }

}

uint32_t Render::applyEffects (uint32_t color, double light, double distanceRatio) {
    uint32_t r = (color & 0x000000ff);
    uint32_t g = (color & 0x0000ff00) >>  8;
    uint32_t b = (color & 0x00ff0000) >> 16;
    // sky color
    uint32_t skyR = 135;
    uint32_t skyG = 206;
    uint32_t skyB = 235;
    r = (r * light) / 100;
    g = (g * light) / 100;
    b = (b * light) / 100;
    if (distanceRatio < 0.05) {
        return rgba(r, g, b, 255);
    } else {
        r = r * (1 - distanceRatio) + skyR * distanceRatio;
        g = g * (1 - distanceRatio) + skyG * distanceRatio;
        b = b * (1 - distanceRatio) + skyB * distanceRatio;
        return rgba(r, g, b, 255);
    }
}


void Render::workerLoop(Chan<WorkerCommand> inputChan, Chan<RenderCommand> mainThread) {
    RenderCommand finished;
    finished.type = FinishedRendering;
    while (true) {
        WorkerCommand cmd = inputChan.read();

        if (cmd.type == QuitThread) {
            return;
        }
        if (cmd.type == StartRendering) {
            uint32_t* data = cmd.data.startRendering.buffer;
            RenderConfig cfg = cmd.data.startRendering.cfg;
            std::vector<uint32_t> hiddeny(width, height);
            double sinang = sin(camera.angle);
            double cosang = cos(camera.angle);

            double deltaz = 0.25;
            for (double z=1; z<camera.distance; z+=deltaz) {
                double plx =  -cosang * z - sinang * z;
                double ply =   sinang * z - cosang * z;
                double prx =   cosang * z - sinang * z;
                double pry =  -sinang * z - cosang * z;

                double dx = (prx - plx) / width;
                double dy = (pry - ply) / width;

                plx += camera.x;
                ply += camera.y;

                double invz = (1 / z) * 240;
                double dR = z / camera.distance;
                double fogRatio;
                if (dR < 0.5) {
                    fogRatio = 0;
                } else {
                    fogRatio = (dR - 0.5) * (1/0.5);
                }

                for (uint32_t i=0; i<width; i++) {
                    TileData tile = map.get(plx, ply);
                    double height = (camera.height - (double)tile.height) * invz + camera.horizon;
                    drawVLine(data, i, height, hiddeny[i], applyEffects(tile.color, tile.light, fogRatio));
                    if (height < hiddeny[i]) hiddeny[i] = height;
                    plx += dx;
                    ply += dy;
                }
                deltaz = 2 * dR * dR + 0.5 * dR + 0.25;
            }

            mainThread.write(finished);
        }
    }
}

struct RenderThreadInfo {
    std::thread th;
    Chan<WorkerCommand> workerChan;
    Chan<RenderCommand> renderChan;
    RenderConfig cfg;
};

void Render::renderLoop() {

    const uint8_t threadCount = 1;
    const int thWidth = width / threadCount;
    std::vector<RenderThreadInfo> threads;

    threads.resize(threadCount);
    for (int i=0; i < threadCount; i++) {
        threads[i].th = std::thread(&Render::workerLoop, &*this, threads[i].workerChan, threads[i].renderChan);
        threads[i].cfg.startWidth = i * thWidth;
        threads[i].cfg.endWidth = (i+1) * thWidth;
    }

    while (keepRender) {
        uint32_t* data = finishRender();
        std::chrono::time_point<std::chrono::steady_clock> renderStartTime = std::chrono::steady_clock::now();
    
        updateCamera(renderStartTime);
        map.setCameraPosition(camera.x, camera.y); 
 
        renderSky(data, width, height);

        for (int i=0; i<threadCount; i++) {
            WorkerCommand cmd;
            cmd.type = StartRendering;
            cmd.data.startRendering.buffer = data;
            cmd.data.startRendering.cfg = threads[i].cfg;
            threads[i].workerChan.write(cmd);
        }

        for (int i=0; i<threadCount; i++) {
            RenderCommand cmd = threads[i].renderChan.read();
            if (cmd.type != FinishedRendering) {
                std::cout << "Error while rendering" << std::endl;
            }
        }

        
        std::chrono::time_point<std::chrono::steady_clock> renderFinalTime = std::chrono::steady_clock::now();
        const std::chrono::duration<double, std::milli> renderTime = renderFinalTime - renderStartTime;
        debug.render.newEntry(renderTime.count());

        lastFrame = renderStartTime;
    }
}