// Add these definitions before any includes to prevent Windows API conflicts
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER

// Include Windows.h first to gain control over its namespace
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif

#include <functional>
#include <string>
#include <cmath>
#include <iostream>
// Include raylib directly but don't use its namespace
#include <raylib.h>
#include <mutex>  // For thread synchronization
#include <queue>  // For the thread-safe image queue

//#include "bella_sdk/bella_engine.h"
//#include "dl_core/dl_fs.h"

#include "../bella_engine_sdk/src/bella_sdk/bella_engine.h" // For rendering and scene creation in Bella
#include "../bella_engine_sdk/src/dl_core/dl_main.inl" // Core functionality from the Diffuse Logic engine
#include "../oom/oom_license.h" // common misc code
#include "../oom/oom_bella_long.h"    // common misc code
#include "../oom/oom_bella_engine.h"    // common misc code

// Create namespace aliases for raylib types that conflict with bella_sdk
namespace rl {
    // Explicitly alias raylib types that conflict with bella_sdk
    using Image = ::Image;
    using Texture2D = ::Texture2D;
    using Vector2 = ::Vector2;
    using Color = ::Color;
    
    // Add function aliases - make sure to include ALL raylib functions you use
    using ::InitWindow;
    using ::CloseWindow;
    using ::IsWindowReady;
    using ::WindowShouldClose;
    using ::IsWindowResized;
    using ::GetScreenWidth;
    using ::GetScreenHeight;
    using ::SetTargetFPS;
    using ::SetConfigFlags;
    using ::BeginDrawing;
    using ::EndDrawing;
    using ::ClearBackground;
    using ::DrawText;
    using ::DrawTextureEx;
    using ::LoadImage;
    using ::UnloadImage;
    using ::LoadTextureFromImage;
    using ::UnloadTexture;
    using ::GetMousePosition;
    using ::GetMouseWheelMove;
    using ::IsMouseButtonPressed;
    using ::IsMouseButtonReleased;
    using ::ShowCursor;
}

// Define a callback type for receiving image data from the path tracer
using OnImageCallback = std::function<void(const unsigned char* data, int width, int height, int channels)>;

// Structure to hold image data in the queue
// This allows us to safely pass image data between threads
struct ImageData {
    unsigned char* data;
    int width;
    int height;
    int channels;
    
    ImageData(unsigned char* d, int w, int h, int c) 
        : data(d), width(w), height(h), channels(c) {}
};

class PathTracerPreview {
private:
    // Window properties
    int screenWidth;
    int screenHeight;
    const char* windowTitle;
    
    // Image/texture properties - use raylib's Texture2D
    rl::Texture2D texture;
    bool imageLoaded;
    float imageScale;
    
    // THREAD SAFETY: These members handle safe communication between threads
    // The mutex protects access to the queue, ensuring only one thread can modify it at a time
    std::mutex queueMutex;
    // The queue stores image data that needs to be processed by the main thread
    // This is crucial because OpenGL operations (like texture creation) must happen on the main thread
    std::queue<ImageData> imageQueue;
    
    // Callback for receiving image data
    OnImageCallback onImageCallback;
    
    // Mouse interaction properties
    bool orbiting = false;
    bool panning = false;
    float orbitSpeed = 0.5f;
    float panSpeed = 0.01f;
    rl::Vector2 prevMousePos = {0, 0};
    dl::bella_sdk::Engine* engine = nullptr;  // Reference to the bella engine for camera control
    
    // Store initial camera state for reset functionality
    bool hasInitialCamera = false;
    // We won't store the transform directly since the API doesn't support it
    // Instead, we'll just remember that we've initialized

public:
    PathTracerPreview(int width, int height, const char* title) 
        : screenWidth(width), screenHeight(height), windowTitle(title), 
          texture({0}), imageLoaded(false), imageScale(1.0f) {
        
        // Set configuration flags before initializing window
        rl::SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
        
        // Initialize window
        rl::InitWindow(screenWidth, screenHeight, windowTitle);
        
        // Check if window was initialized successfully
        if (!rl::IsWindowReady()) {
            std::cerr << "ERROR: Failed to initialize window" << std::endl;
            return;
        }
        
        // Set target FPS
        rl::SetTargetFPS(60);
        
        // THREAD SAFETY: Set up the callback that will be called by the path tracer
        // Instead of directly updating the image (which would create textures in a non-main thread),
        // we queue the image data for later processing by the main thread
        onImageCallback = [this](const unsigned char* data, int width, int height, int channels) {
            this->queueImageData(data, width, height, channels);
        };
        
        //std::cout << "Window initialized successfully" << std::endl;
    }
    
    // Set the engine reference for camera control
    void setEngine(dl::bella_sdk::Engine* engineRef) {
        engine = engineRef;
        
        // Mark that we have an initial camera state
        /*
        if (engine) {
            storeInitialCameraTransform();
        }
        */
    }
    
    ~PathTracerPreview() {
        // Clean up resources
        if (texture.id != 0) rl::UnloadTexture(texture);
        
        // THREAD SAFETY: Clean up any remaining image data in the queue
        clearImageQueue();
        
        rl::CloseWindow();
    }
    
    // Get the callback that the path tracer should call when new image data is available
    OnImageCallback getCallback() const {
        return onImageCallback;
    }
    
    // THREAD SAFETY: Queue image data to be processed by the main thread
    // This method can be safely called from any thread
    // It copies the image data and adds it to a thread-safe queue
    void queueImageData(const unsigned char* data, int width, int height, int channels) {
        if (!data || width <= 0 || height <= 0 || channels <= 0) {
            std::cerr << "ERROR: Invalid image data parameters" << std::endl;
            return;
        }
        
        // Create a copy of the data to ensure it remains valid even after the caller frees their copy
        size_t dataSize = width * height * channels;
        unsigned char* dataCopy = new unsigned char[dataSize];
        std::memcpy(dataCopy, data, dataSize);
        
        // THREAD SAFETY: Add to queue with lock to prevent race conditions
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            imageQueue.push(ImageData(dataCopy, width, height, channels));
        }
    }
    
    // THREAD SAFETY: Process any queued image data - call this from the main thread
    // This is a key method that bridges between threads:
    // 1. The bella engine thread adds data to the queue via queueImageData()
    // 2. The main thread calls this method to safely retrieve and process that data
    void processImageQueue() {
        ImageData imageData(nullptr, 0, 0, 0);
        bool hasData = false;
        
        // THREAD SAFETY: Get the next image data from the queue with proper locking
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!imageQueue.empty()) {
                imageData = imageQueue.front();
                imageQueue.pop();
                hasData = true;
            }
        }
        
        // Process the image data if we got any
        // This happens in the main thread where OpenGL operations are safe
        if (hasData) {
            updateImage(imageData.data, imageData.width, imageData.height, imageData.channels);
            delete[] imageData.data; // Clean up the data copy
        }
    }
    
    // THREAD SAFETY: Clear the image queue
    // This ensures we don't leak memory if there are pending images when the program exits
    void clearImageQueue() {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!imageQueue.empty()) {
            ImageData& imageData = imageQueue.front();
            delete[] imageData.data;
            imageQueue.pop();
        }
    }
    
    // Update the displayed image with new data from the path tracer
    // IMPORTANT: This method must ONLY be called from the main thread
    // because it creates OpenGL textures which are context-dependent
    void updateImage(const unsigned char* data, int width, int height, int channels) {
        
        try {
            // Check if data is valid
            if (!data) {
                std::cerr << "ERROR: Data pointer is NULL" << std::endl;
                return;
            }
            
            // Unload existing texture if any
            if (texture.id != 0) {
                rl::UnloadTexture(texture);
                texture = {0};
            }
            
            // Create a copy of the data
            unsigned char* dataCopy = new unsigned char[width * height * 4]; // Always use 4 channels (RGBA)
            
            // Convert the data to RGBA format
            for (int i = 0; i < width * height; i++) {
                int srcIdx = i * channels;
                int destIdx = i * 4;
                
                if (channels >= 3) {
                    dataCopy[destIdx] = data[srcIdx];     // R
                    dataCopy[destIdx + 1] = data[srcIdx + 1]; // G
                    dataCopy[destIdx + 2] = data[srcIdx + 2]; // B
                    dataCopy[destIdx + 3] = (channels >= 4) ? data[srcIdx + 3] : 255; // A
                } else if (channels == 1) {
                    // Grayscale
                    dataCopy[destIdx] = dataCopy[destIdx + 1] = dataCopy[destIdx + 2] = data[srcIdx];
                    dataCopy[destIdx + 3] = 255;
                }
            }
            
            // Create a simple image using the raw data
            
            // Create a new image with the data
            rl::Image image = {0};
            image.data = dataCopy;
            image.width = width;
            image.height = height;
            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            
            // THREAD SAFETY: Load texture from the image
            // This is now safe because we're in the main thread with valid OpenGL context
            // This was the source of our segfault when called from a different thread
            texture = rl::LoadTextureFromImage(image);
            
            // Unload the image data (texture keeps a copy)
            rl::UnloadImage(image);
            
            if (texture.id == 0) {
                std::cerr << "ERROR: Failed to create texture" << std::endl;
                return;
            }
            
            
            // Update display settings
            imageLoaded = true;
            
            // Calculate scale to fit the image in the window with some padding
            float scaleX = static_cast<float>(screenWidth - 40) / texture.width;
            float scaleY = static_cast<float>(screenHeight - 40) / texture.height;
            imageScale = (scaleX < scaleY) ? scaleX : scaleY;  // Use the smaller scale
            
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Exception in updateImage: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "ERROR: Unknown exception in updateImage" << std::endl;
        }
    }
    
    // Main loop - call this to run the preview window
    void run() {
        // Store previous window size to detect resizing
        int prevScreenWidth = screenWidth;
        int prevScreenHeight = screenHeight;
        
        while (!rl::WindowShouldClose()) {
            // THREAD SAFETY: Process any queued image data in the main thread
            // This is where we safely handle the image data that was queued by other threads
            processImageQueue();
            
            // Check if window has been resized
            if (rl::IsWindowResized()) {
                // Update screen dimensions
                screenWidth = rl::GetScreenWidth();
                screenHeight = rl::GetScreenHeight();
                
                // Recalculate image scale to fit the new window size
                if (imageLoaded && texture.id != 0) {
                    float scaleX = static_cast<float>(screenWidth - 40) / texture.width;
                    float scaleY = static_cast<float>(screenHeight - 40) / texture.height;
                    imageScale = (scaleX < scaleY) ? scaleX : scaleY;  // Use the smaller scale
                }
            }
            
            // Update
            if (imageLoaded) {
                // Allow zooming with mouse wheel
                float wheelMove = rl::GetMouseWheelMove();
                if (wheelMove != 0.0f) {
                    if (engine && engine->rendering()) {
                        dl::bella_sdk::Scene::EventScope eventScope(engine->scene());
                        // Create a Vec2 with y component only for dolly effect
                        dl::Vec2 dollyDelta;
                        dollyDelta.y = wheelMove * 0.8;
                        dl::bella_sdk::zoomCamera( engine->scene().cameraPath(), dollyDelta, true );
                    }
                }
                
                // Handle mouse interaction for camera orbiting
                handleMouseInteraction();
            }
            
            // Draw
            rl::BeginDrawing();
            
            rl::ClearBackground(RAYWHITE);
            
            if (imageLoaded && texture.id != 0) {
                // Draw the texture centered in the window
                rl::DrawTextureEx(
                    texture, 
                    { 
                        static_cast<float>(screenWidth)/2 - texture.width*imageScale/2, 
                        static_cast<float>(screenHeight)/2 - texture.height*imageScale/2 
                    }, 
                    0, imageScale, WHITE
                );
                
                // Display the current scale factor
                //DrawText(TextFormat("Scale: %.2fx", imageScale), 10, screenHeight - 30, 20, DARKGRAY);
                
                // Display orbit/pan status and controls
                //if (orbiting) {
                //    DrawText("Orbiting Camera", 10, screenHeight - 60, 20, RED);
                //} else if (panning) {
                //    DrawText("Panning Camera", 10, screenHeight - 60, 20, BLUE);
                //}
                
                // Display control instructions
                //DrawText("Mouse Controls:", 10, 10, 20, DARKGRAY);
                //DrawText("- Left Click + Drag: Orbit Camera", 10, 35, 18, DARKGRAY);
                //DrawText("- Middle Click + Drag: Pan Camera", 10, 60, 18, DARKGRAY);
                //DrawText("- Right Click: Reset Camera", 10, 85, 18, DARKGRAY);
                //DrawText("- Mouse Wheel: Zoom Image", 10, 110, 18, DARKGRAY);
                //DrawText("- Shift + Mouse Wheel: Dolly Camera", 10, 135, 18, DARKGRAY);
            } else {
                // No image loaded yet
                rl::DrawText("Waiting for Bella to render...", screenWidth/2 - 150, screenHeight/2 - 10, 20, DARKGRAY);
            }
            
            rl::EndDrawing();
        }
    }
    
    // Handle mouse interaction for camera orbiting
    void handleMouseInteraction() {
        // Only process mouse interaction if we have a valid engine reference
        if (!engine) return;
        
        // Check for mouse button press/release for orbiting (left button)
        if (rl::IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            orbiting = true;
            panning = false;  // Ensure we're not doing both at once
            prevMousePos = rl::GetMousePosition();
        } else if (rl::IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            orbiting = false;
        }
        
        // Check for mouse button press/release for panning (middle button)
        if (rl::IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
            panning = true;
            orbiting = false;  // Ensure we're not doing both at once
            prevMousePos = rl::GetMousePosition();
        } else if (rl::IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) {
            panning = false;
        }
        
        // Check for right-click to reset camera
        /*if (rl::IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            if (engine->rendering()) {
                // Use our custom reset camera function
                resetCamera();
            }
        }*/
        
        // Handle orbiting when left mouse is dragged
        if (orbiting) {
            rl::Vector2 currentMousePos = rl::GetMousePosition();
            
            // Calculate delta movement
            float deltaX = (currentMousePos.x - prevMousePos.x) * orbitSpeed;
            float deltaY = (currentMousePos.y - prevMousePos.y) * orbitSpeed;
            
            // Only orbit if there's actual movement
            if (deltaX != 0.0f || deltaY != 0.0f) {
                // Create a Vec2 for the delta (using bsdk's Vec2)
                // Fix: Initialize Vec2 properly according to its API
                dl::Vec2 delta;
                delta.x = deltaX;
                delta.y = deltaY;
                
                // Orbit the camera - similar to the C# implementation
                if (engine->rendering()) {
                    // Create an EventScope to batch scene updates efficiently
                    // This collects all scene changes within this scope and applies them together
                    // when the eventScope object is destroyed at the end of this block.
                    // It improves performance and ensures consistency of multiple scene changes.
                    dl::bella_sdk::Scene::EventScope eventScope(engine->scene());
                    // Use the bsdk namespace to avoid ambiguity with Path
                    dl::bella_sdk::orbitCamera(engine->scene().cameraPath(), delta);
                }
                
                // Update previous position for next frame
                prevMousePos = currentMousePos;
            }
        }
        
        // Handle panning when middle mouse is dragged
        if (panning) {
            rl::Vector2 currentMousePos = rl::GetMousePosition();
            
            // Calculate delta movement
            float deltaX = (currentMousePos.x - prevMousePos.x) * panSpeed;
            float deltaY = (currentMousePos.y - prevMousePos.y) * panSpeed;
            
            // Only pan if there's actual movement
            if (deltaX != 0.0f || deltaY != 0.0f) {
                // Create a Vec2 for the delta (using bsdk's Vec2)
                dl::Vec2 delta;
                delta.x = deltaX;
                delta.y = deltaY;
                
                if (engine->rendering()) {
                    // Create an EventScope to batch scene updates efficiently
                    // This collects all scene changes within this scope and applies them together
                    // when the eventScope object is destroyed at the end of this block.
                    // It improves performance and ensures consistency of multiple scene changes.
                    dl::bella_sdk::Scene::EventScope eventScope(engine->scene());
                    dl::bella_sdk::panCamera(engine->scene().cameraPath(), delta, true);
                }
                
                // Update previous position for next frame
                prevMousePos = currentMousePos;
            }
        }
    }
    
    // Simulate receiving data from the path tracer (for testing)
    void simulateDataFromPathTracer(const char* filename) {
        // Load an image from file - use raylib's Image type
        rl::Image image = rl::LoadImage(filename);
        if (image.data != NULL) {
            // Call our callback with the image data
            onImageCallback(
                static_cast<const unsigned char*>(image.data),
                image.width,
                image.height,
                image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE ? 1 :
                image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA ? 2 :
                image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8 ? 3 : 4
            );
            rl::UnloadImage(image);
        }
    }
};

// Forward declaration
class PathTracerPreview;

// Custom engine observer that connects bsdk's Image to our PathTracerPreview
struct BellaEngineObserver : public dl::bella_sdk::EngineObserver {
private:
    PathTracerPreview* preview;

public:
    BellaEngineObserver(PathTracerPreview* preview) : preview(preview) {}

    void onStarted(dl::String pass) override {
        //logInfo("Started pass %s", pass.buf());
    }
    
    void onStatus(dl::String pass, dl::String status) override {
        //logInfo("%s [%s]", status.buf(), pass.buf());
    }
    
    void onProgress(dl::String pass, dl::bella_sdk::Progress progress) override {
        //logInfo("%s [%s]", progress.toString().buf(), pass.buf());
    }
    
    // This is the key method that receives images from the bella engine
    // IMPORTANT: This method is called from the bella engine's thread, NOT the main thread
    void onImage(dl::String pass, dl::bella_sdk::Image image) override {
        //logInfo("Received image from bella: %d x %d", (int)image.width(), (int)image.height());
        
        // Get the dimensions of the image
        int width = (int)image.width();
        int height = (int)image.height();
        
        if (!preview) {
            std::cerr << "ERROR: Preview pointer is NULL" << std::endl;
            return;
        }
        
        try {
            // Create a buffer for our RGBA data
            unsigned char* buffer = new unsigned char[width * height * 4];
            
            // Get the raw RGBA data pointer - rgba8() returns Rgba8* (RgbaT<unsigned char>*)
            // No mutex needed as the developers confirmed the data survives within this callback
            dl::Rgba8* rgba_data = image.rgba8();
            
            if (!rgba_data) {
                std::cerr << "ERROR: rgba8() returned NULL" << std::endl;
                delete[] buffer;
                return;
            }
            
            // Direct memory copy for optimal performance
            std::memcpy(buffer, rgba_data, width * height * 4);
            
            // THREAD SAFETY: Instead of directly calling updateImage (which would create textures in the wrong thread),
            // use the callback which will queue the data for processing by the main thread
            try {
                if (preview->getCallback()) {
                    // This will queue the data for later processing in the main thread
                    preview->getCallback()(buffer, width, height, 4);
                    //std::cout << "Image data queued successfully" << std::endl;
                } else {
                    std::cerr << "ERROR: Preview callback is NULL" << std::endl;
                    delete[] buffer;
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception in queueing image data: " << e.what() << std::endl;
                delete[] buffer;  // Clean up if queueing throws
            } catch (...) {
                std::cerr << "Unknown exception in queueing image data" << std::endl;
                delete[] buffer;  // Clean up if queueing throws
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in onImage: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in onImage" << std::endl;
        }
    }
    
    void onError(dl::String pass, dl::String msg) override {
        //logError("%s [%s]", msg.buf(), pass.buf());
    }
    
    void onStopped(dl::String pass) override {
        //logInfo("Stopped %s", pass.buf());
    }
};

//#include "dl_core/dl_main.inl"
int DL_main(dl::Args& args)
{
    try {
        SetTraceLogLevel(LOG_ERROR); 
        // Set raylib configuration flags before creating the window
        rl::SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
        PathTracerPreview preview(400, 400, "poomer-raylib-bella_onimage");
        if (!rl::IsWindowReady()) {
            std::cerr << "ERROR: Window initialization failed" << std::endl;
            return 1;
        }
        
        // Add a small delay to ensure the OpenGL context is fully set up
        //std::cout << "Waiting for OpenGL context to initialize..." << std::endl;
        for (int i = 0; i < 5; i++) {
            rl::BeginDrawing();
            rl::ClearBackground(RAYWHITE);
            rl::DrawText("Initializing...", 10, 10, 20, DARKGRAY);
            rl::EndDrawing();
        }
        //std::cout << "OpenGL context initialized" << std::endl;
        
        // Initialize the bella engine
        dl::bella_sdk::Engine engine;
        engine.scene().loadDefs();
        engine.enableInteractiveMode();
        engine.enableDisplayTransform();
        
        // Pass the engine reference to the preview window for camera control
        preview.setEngine(&engine);
        
        // Create our custom observer and connect it to the preview window
        BellaEngineObserver engineObserver(&preview);
        engine.subscribe(&engineObserver);

        // Get the preview scene with material sphere
        auto path = dl::bella_sdk::previewPath();
        if (path != "") {
            //std::cout << "Loading scene: " << path.buf() << std::endl;
            //logInfo("Loading scene: %s", path.buf());
            
            // Use the read method to load the scene
            if (!engine.scene().read(path)) {
                std::cerr << "ERROR: Failed to read " << path.buf() << " from " << dl::fs::currentDir().buf() << std::endl;
                dl::logError("Failed to read %s from %s", path.buf(), dl::fs::currentDir().buf());
                return 1;
            }
            
            if (!engine.start()) {
                std::cerr << "ERROR: Engine failed to start" << std::endl;
                dl::logError("Engine failed to start.");
                return 1;
            }
            
            // Store the initial camera state after the scene is loaded and engine started
            //preview.storeInitialCameraTransform();
            
            //std::cout << "Engine started successfully" << std::endl;
        } else {
            // For testing, load a sample image
            preview.simulateDataFromPathTracer("oomer.png");
        }

        // Run the preview window - this will block until the window is closed
        preview.run();
        
        // Clean up
        engine.stop();
        engine.unsubscribe(&engineObserver);
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: Exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FATAL ERROR: Unknown exception in main" << std::endl;
        return 1;
    }
} 