#include <dirent.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>


#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include "frame_reader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

char* pixfmt2str(int pixel_format) {
    char* fmt;
    switch (pixel_format) {
        // lets start with the common ones
        case V4L2_PIX_FMT_YUYV:
            fmt = "YUYV";
            break;
        case V4L2_PIX_FMT_MJPEG:
            fmt = "MJPEG";
            break;
        case V4L2_PIX_FMT_NV12:
            fmt = "NV12";
            break;
        // now not so common ones (RGB/BGR)
        case V4L2_PIX_FMT_BGRA32:
            fmt = "BGRA32";
            break;
        case V4L2_PIX_FMT_BGR24:
            fmt = "BGR24";
            break;
        case V4L2_PIX_FMT_RGBA32:
            fmt = "RGBA32";
            break;
        case V4L2_PIX_FMT_RGB24:
            fmt = "RGB24";
            break;
        // now not so common ones (YUV, YVU)
        case V4L2_PIX_FMT_YVU420M:
            fmt = "YVU420M";
            break;
        case V4L2_PIX_FMT_YUV48_12:
            fmt = "YUV48_12";
            break;
        case V4L2_PIX_FMT_YUV422P:
            fmt = "YUV422P";
            break;
        // now not so common ones (MPEG)
        case V4L2_PIX_FMT_MPEG:
            fmt = "MPEG";
            break;
        case V4L2_PIX_FMT_MPEG1:
            fmt = "MPEG1";
            break;
        case V4L2_PIX_FMT_MPEG2:
            fmt = "MPEG2";
            break;
        case V4L2_PIX_FMT_MPEG4:
            fmt = "MPEG4";
            break;
        // unknown
        default:
            fmt = "unknown";
            break;
    }
    return fmt;
}

#define str2pixfmt_case(test, fmt) \
    if (strcmp(str, test) == 0) {  \
        return fmt;                \
    }

int str2pixfmt(char* str) {
    // lets start with the common ones
    str2pixfmt_case("YUYV", V4L2_PIX_FMT_YUYV);
    str2pixfmt_case("MJPEG", V4L2_PIX_FMT_MJPEG);
    str2pixfmt_case("NV12", V4L2_PIX_FMT_NV12);
    // now not so common ones (RGB/BGR)
    str2pixfmt_case("BGR32", V4L2_PIX_FMT_BGR32);
    str2pixfmt_case("RGBA32", V4L2_PIX_FMT_RGBA32);
    str2pixfmt_case("RGB24", V4L2_PIX_FMT_RGB24);
    // now not so common ones (YUV, YVU)
    str2pixfmt_case("YVU420M", V4L2_PIX_FMT_YVU420M);
    str2pixfmt_case("YUV48_12", V4L2_PIX_FMT_YUV48_12);
    str2pixfmt_case("YUV422P", V4L2_PIX_FMT_YUV422P);
    // now not so common ones (MPEG)
    str2pixfmt_case("MPEG", V4L2_PIX_FMT_MPEG);
    str2pixfmt_case("MPEG1", V4L2_PIX_FMT_MPEG1);
    str2pixfmt_case("MPEG2", V4L2_PIX_FMT_MPEG2);
    str2pixfmt_case("MPEG4", V4L2_PIX_FMT_MPEG4);

    return -1;
}

char** list_video_devices() {
    DIR* dir;
    struct dirent* entry;
    dir = opendir("/dev");
    if (!dir) {
        return NULL;
    }

    size_t len = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "video", 5) == 0) {
            len++;
        }
    }

    rewinddir(dir);

    char** devices = malloc(sizeof(char*) * (len + 1));
    devices[len] = NULL;

    size_t index = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "video", 5) == 0) {
            devices[index] = malloc(sizeof(char) * (5 + strlen(entry->d_name) + 1));
            snprintf(devices[index], 5 + strlen(entry->d_name) + 1, "/dev/%s", entry->d_name);
            index++;
        }
    }

    closedir(dir);

    return devices;
}

void free_cpp(char** ls) {
    size_t index = 0;
    while (ls[index] != NULL) {
        free(ls[index++]);
    }
    free(ls);
}

size_t cpplen(char** ls) {
    size_t index = 0;
    while (ls[index++] != NULL);
    return index;
}

char** list_devices(char** video_devices) {
    char** devices = calloc(cpplen(video_devices), sizeof(char*));

    int fd = 0;
    struct v4l2_capability vid_caps = {0};
    size_t wrt_ptr = 0;
    for (size_t i = 0; video_devices[i] != NULL; i++) {
        fd = open(video_devices[i], O_RDWR);
        if (fd >= 0) {
            int device_read_status = ioctl(fd, VIDIOC_QUERYCAP, &vid_caps);
            if (device_read_status != -1 && (strcmp("uvcvideo", (char*)vid_caps.driver) == 0 || strcmp("v4l2 loopback", (char*)vid_caps.driver) == 0)) {
                devices[wrt_ptr++] = strdup(video_devices[i]);
            }
        }
    }
        return devices;
}

bool is_webcam_device(char* dev) {
    bool is_lbdev = false;

    char** video_devices = list_video_devices();
    char** devices = list_devices(video_devices);
    for (size_t i = 0; devices[i] != NULL; i++) {
        if (strcmp(devices[i], dev) == 0) {
            is_lbdev = true;
            break;
        }
    }

    free_cpp(video_devices);
    free_cpp(devices);

    return is_lbdev;
}

char** list_capture_methods(char* dev) {
    char** methods = calloc(3+1, sizeof(char*));
    size_t index = 0;
    int fd;
    struct v4l2_capability vid_caps = {0};

    fd = open(dev, O_RDWR);

    if (fd < 0) {
        printf("Failed to open camera device!\n");
        free(methods);
        return NULL;
    }

    // query device capabilities
    int query_cam = ioctl(fd, VIDIOC_QUERYCAP, &vid_caps);
    if (query_cam == -1) {
        printf("Failed to query camera!\n");
        free(methods);
        return NULL;
    }

    if (vid_caps.capabilities & V4L2_CAP_READWRITE) {
        methods[index++] = strdup("READ");
    }

    if (vid_caps.capabilities & V4L2_MEMORY_MMAP) {
        methods[index++] = strdup("MMAP");
    }

    if (vid_caps.capabilities & V4L2_MEMORY_USERPTR) {
        methods[index++] = strdup("USERPTR");
    }

    if (vid_caps.capabilities & V4L2_MEMORY_DMABUF) {
        methods[index++] = strdup("DMABUF");
    }

    if (vid_caps.capabilities & V4L2_MEMORY_OVERLAY) {
        methods[index++] = strdup("OVERLAY");
    }

    return methods;
}

bool best_supported_capture_method(int fd, enum supported_capture_mode* cm) {

    struct v4l2_capability vid_caps = {0};

    // query device capabilities
    int query_cam = ioctl(fd, VIDIOC_QUERYCAP, &vid_caps);
    if (query_cam == -1) {
        printf("Failed to query camera!\n");
        return NULL;
    }

    if (vid_caps.capabilities & V4L2_MEMORY_DMABUF) {
        // not supported
    }

    if (vid_caps.capabilities & V4L2_MEMORY_MMAP) {
        *cm = capture_mode_mmap;
        return true;
    }

    if (vid_caps.capabilities & V4L2_MEMORY_USERPTR) {
        // not supported
    }

    if (vid_caps.capabilities & V4L2_MEMORY_OVERLAY) {
        // not supported
    }

    if (vid_caps.capabilities & V4L2_CAP_READWRITE) {
        *cm = capture_mode_read;
        return true;
    }

    return false;
}

char** list_device_formats(char* dev) {
    char** fmts;
    int fd = 0;
    struct v4l2_fmtdesc fmt = {0};

    // open the device
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        printf("Failed to open camera device!\n");
        return NULL;
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fmt.index = 0;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0 && ++fmt.index);

    fmts = calloc(fmt.index + 1, sizeof(char*));

    fmt.index = 0;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
        fmts[fmt.index++] = strdup(pixfmt2str(fmt.pixelformat));
    }

    close(fd);
    return fmts;
}

char** list_device_resolutions(char* dev, char* search_format) {
    char** resolutions;
    int fd = 0;
    struct v4l2_frmsizeenum frmsize = {0};

    // open the device
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        printf("Failed to open camera device!\n");
        return NULL;
    }

    frmsize.pixel_format = str2pixfmt(search_format);
    frmsize.index = 0;
    while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0 && ++frmsize.index);

    resolutions = calloc(frmsize.index + 1, sizeof(char*));

    frmsize.index = 0;
    while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
        resolutions[frmsize.index] = calloc(20, sizeof(char));  // 20 is a guess
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            snprintf(resolutions[frmsize.index], 20, "%dx%d", frmsize.discrete.width, frmsize.discrete.height);
        } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            snprintf(resolutions[frmsize.index], 20, "%dx%d", frmsize.stepwise.max_width, frmsize.stepwise.max_height);
        }
        frmsize.index++;
    }

    close(fd);
    return resolutions;
}

bool is_supported_format_resolution (char* dev, char* format, char* resolution) {
    bool found = false;
    char** resolutions = list_device_resolutions(dev, format);
    for (size_t i = 0; resolutions[i] != NULL; i++) {
        if (strcmp(resolutions[i], resolution) == 0) {
            found = true;
            break;
        }
    }
    free_cpp(resolutions);
    return found;
}

void print_format(struct v4l2_format* vid_format) {
    char* fmt = pixfmt2str(vid_format->fmt.pix.pixelformat);
    printf(
        "vid_format->type                 =%d \n"
        "vid_format->fmt.pix.width        =%d \n"
        "vid_format->fmt.pix.height       =%d \n"
        "vid_format->fmt.pix.pixelformat  =%d (%s) \n"
        "vid_format->fmt.pix.sizeimage    =%d \n"
        "vid_format->fmt.pix.field        =%d \n"
        "vid_format->fmt.pix.bytesperline =%d \n"
        "vid_format->fmt.pix.colorspace   =%d \n",
        vid_format->type, vid_format->fmt.pix.width, vid_format->fmt.pix.height, vid_format->fmt.pix.pixelformat, fmt,
        vid_format->fmt.pix.sizeimage, vid_format->fmt.pix.field, vid_format->fmt.pix.bytesperline,
        vid_format->fmt.pix.colorspace);
}

void parse_resolution(char* res, size_t* width, size_t* height) {
    // most of this is safe if res was checked with is_supported_format_resolution
    char* res_pair = strdup(res);
    char* res_width = res_pair;
    char* res_height = strstr(res_pair, "x");
    *res_height = '\0';
    res_height++;

    *width = atoi(res_width);
    *height = atoi(res_height);
    free(res_pair);
}

// https://wiki.delphigl.com/index.php/glBegin
#define OGLBQ0 -1, -1
#define OGLBQ1 -1, 1
#define OGLBQ2 1, 1
#define OGLBQ3 1, -1

#define OGLTX0 0, 0
#define OGLTX1 0, 1
#define OGLTX2 1, 1
#define OGLTX3 1, 0

int read_texture(char* dev, char* fmt, char* res) {



    // The plan
    // 1. Initialize the Camera Device
    // 2. Initialize the Frame Reader
    // 3. Initialize OpenGL
    // 4. Read images from the camera
    // 5. Transfer image to OpenGL texture
    // 6. Draw window

    int fd = 0;
    struct v4l2_capability vid_caps = {0};
    struct v4l2_format vid_format = {0};
    size_t width = 0;
    size_t height = 0;

    struct frame_reader* fr;

    parse_resolution(res, &width, &height);

    // STEP 1.: Initialize the Camera Device

    // open the device
    fd = open(dev, O_RDWR);

    if (fd < 0) {
        printf("Failed to open camera device!\n");
        return 1;
    }

    // query device capabilities
    int query_cam = ioctl(fd, VIDIOC_QUERYCAP, &vid_caps);
    if (query_cam == -1) {
        printf("Failed to query camera!\n");
        return 1;
    }

    // loading current fmt from device (this is only a sanity check)
    vid_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int read_fmt = ioctl(fd, VIDIOC_G_FMT, &vid_format);
    if (read_fmt == -1) {
        printf("Failed to read fmt\n");
        return 1;
    }

    vid_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vid_format.fmt.pix.width = width;
    vid_format.fmt.pix.height = height;
    vid_format.fmt.pix.pixelformat = str2pixfmt(fmt);
    vid_format.fmt.pix.field = V4L2_FIELD_NONE;
    vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;


    int put_fmt = ioctl(fd, VIDIOC_S_FMT, &vid_format);
    if (put_fmt == -1) {
        printf("Failed to write fmt\n");
        return 1;
    }

    if (vid_format.fmt.pix.width != width || vid_format.fmt.pix.height != height) {
        printf("Failed to set requested resoluition of %zux%zu, driver proposed %dx%d\n", width, height, vid_format.fmt.pix.width, vid_format.fmt.pix.height);
        return 1;
    }

    print_format(&vid_format);

    // STEP 2.: Initialize the Frame Reader
    enum supported_capture_mode s_mode = capture_mode_mmap;
    if (!best_supported_capture_method(fd, &s_mode)) {
        printf("No compatible captore method found\n");
        return 1;
    }

    printf("Using capture method %s\n", s_mode == capture_mode_mmap? "MMAP":"READ");

    enum supported_capture_format s_fmt;
    switch(str2pixfmt(fmt)) {
        case V4L2_PIX_FMT_RGB24:
            s_fmt = capture_format_RGB24;
            break;
        case V4L2_PIX_FMT_YUYV:
            s_fmt = capture_format_YUYV;
            break;
        case V4L2_PIX_FMT_MJPEG:
            s_fmt = capture_format_MJPEG;
            break;
        case V4L2_PIX_FMT_NV12:
            s_fmt = capture_format_NV12;
            break;
        default:
            printf("Format %s is currently not implemented", fmt);
            return 1;
    }

    fr = reader_new(fd, s_mode, s_fmt, width, height, vid_format.fmt.pix.sizeimage);

    // STEP 3.: Initialize the OpenGL
    GLFWwindow* gl_ctx;
    GLenum err;

    GLuint texture;

    // create opengl context + window
    glfwInit();
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    gl_ctx =
        glfwCreateWindow(width, height, "Display OpenGL texture populated from video device", NULL, NULL);
    if (!gl_ctx) {
        printf("Failed to create opengl context\n");
        return 1;
    }

    glfwMakeContextCurrent(gl_ctx);
    err = glewInit();
    if (GLEW_OK != err) {
        printf("Failed to init glew\n");
        return 1;
    }

    // generate texture
    glEnable(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    reader_start(fr);

     void* image_data = NULL;

    while (1) {

        // STEP 4. Read images from the camera
        image_data = reader_read_decode_rgb(fr);

        // STEP 5. Transfer image to OpenGL texture
        if (image_data != NULL) {
            glTexImage2D(GL_TEXTURE_2D, 0, fr->texture_format, width, height, 0, fr->texture_format, GL_UNSIGNED_BYTE, image_data);
        }

        // STEP 6. Draw Window
        glViewport(0, 0, width, height);
        glfwSetWindowSize(gl_ctx, width, height);
        glBegin(GL_QUADS);
        glTexCoord2f(OGLTX1);
        glVertex2f(OGLBQ0);
        glTexCoord2f(OGLTX0);
        glVertex2f(OGLBQ1);
        glTexCoord2f(OGLTX3);
        glVertex2f(OGLBQ2);
        glTexCoord2f(OGLTX2);
        glVertex2f(OGLBQ3);

        /*
        glBegin(GL_QUADS);
        glTexCoord2f(OGLTX0);
        glVertex2f(OGLBQ0);
        glTexCoord2f(OGLTX1);
        glVertex2f(OGLBQ1);
        glTexCoord2f(OGLTX2);
        glVertex2f(OGLBQ2);
        glTexCoord2f(OGLTX3);
        glVertex2f(OGLBQ3);
        */
        glEnd();

        glFlush();
        glfwSwapBuffers(gl_ctx);

        //usleep(1.0f / 30 * 1000000.0f);
    }

    // TODO free everything

    reader_stop(fr);


    reader_destroy(fr);
    close(fd);
    return 0;
}

void usage() {
    printf(
        "Usage: main <command>\n\n"
        "Commands:\n"
        "   list-devices                                    list loopback devices\n"
        "   is-webcam <device>                              check is a given device is a webcam\n"
        "   list-capture-methods <device>                              check is a given device is a webcam\n"
        "   list-formats <device>                           list a devices available formats\n"
        "   list-resolutions <device> <format>              list a devices available resolutions for a given format\n"
        "   is-supported <device> <format> <width>x<height> check if a resolution and format is supported by the "
        "device\n"
        "   read-texture <device> <format> <width>x<height> read an image into a opengl texture and display\n");
}

int main(int argc, char* argv[]) {
    (void)argc;

    if (argc == 1) {
        usage();
        return 1;
    }

    if (strcmp("list-devices", argv[1]) == 0) {
        char** video_devices = list_video_devices();
        char** devices = list_devices(video_devices);
        for (size_t i = 0; devices[i] != NULL; i++) {
            printf("%s\n", devices[i]);
        }
        free_cpp(video_devices);
        free_cpp(devices);
        return 0;
    }

    if (strcmp("is-webcam", argv[1]) == 0) {
        if (argc != 3) {
            usage();
            return 1;
        }

        if (is_webcam_device(argv[2])) {
            printf("Device '%s' is a webcam\n", argv[2]);
            return 0;
        } else {
            printf("Device '%s' is NOT a webcam\n", argv[2]);
            return 1;
        }
    }

    if (strcmp("list-capture-methods", argv[1]) == 0) {
        if (argc != 3) {
            usage();
            return 1;
        }
        if (!is_webcam_device(argv[2])) {
            printf("Device '%s' is NOT a webcam\n", argv[2]);
            return 1;
        }
        char** fmts = list_capture_methods(argv[2]);
        for (size_t i = 0; fmts[i] != NULL; i++) {
            printf("%s\n", fmts[i]);
        }
        free_cpp(fmts);

        return 0;
    }

    if (strcmp("list-formats", argv[1]) == 0) {
        if (argc != 3) {
            usage();
            return 1;
        }

        if (!is_webcam_device(argv[2])) {
            printf("Device '%s' is NOT a webcam\n", argv[2]);
            return 1;
        }
        char** fmts = list_device_formats(argv[2]);
        for (size_t i = 0; fmts[i] != NULL; i++) {
            printf("%s\n", fmts[i]);
        }
        free_cpp(fmts);

        return 0;
    }

    if (strcmp("list-resolutions", argv[1]) == 0) {
        if (argc != 4) {
            usage();
            return 1;
        }

        if (!is_webcam_device(argv[2])) {
            printf("Device '%s' is NOT a webcam\n", argv[2]);
            return 1;
        }
        char** resolutions = list_device_resolutions(argv[2], argv[3]);
        for (size_t i = 0; resolutions[i] != NULL; i++) {
            printf("%s\n", resolutions[i]);
        }
        free_cpp(resolutions);

        return 0;
    }

    if (strcmp("is-supported", argv[1]) == 0) {
        if (argc != 5) {
            usage();
            return 1;
        }

        if (!is_webcam_device(argv[2])) {
            printf("Device '%s' is NOT a webcam\n", argv[2]);
            return 1;
        }

        bool found = is_supported_format_resolution(argv[2], argv[3], argv[4]);
        printf("Resolution is %s supported\n", found ? "" : "NOT");
        return found ? 0 : 1;
    }

    if (strcmp("read-texture", argv[1]) == 0) {
        if (argc != 5) {
            usage();
            return 1;
        }

        if (!is_webcam_device(argv[2])) {
            printf("Device '%s' is NOT a v4l2 loopback device\n", argv[2]);
            return 1;
        }

        if(!is_supported_format_resolution(argv[2], argv[3], argv[4])) {
            printf("Resolution is NOT supported\n");
            return 1;
        }

        return read_texture(argv[2], argv[3], argv[4]);
    }

    usage();
    return EXIT_FAILURE;
}

