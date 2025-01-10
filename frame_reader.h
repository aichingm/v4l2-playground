#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "stb_image.h"


enum supported_capture_mode{
    capture_mode_read,
    capture_mode_mmap,
};

enum supported_capture_format{
    capture_format_RGB24,
    capture_format_YUYV,
    capture_format_MJPEG,
    capture_format_NV12,
};

struct frame_reader_buffers {
    void* ptr;
    size_t length;
};

struct frame_reader {
    int fd;
    enum supported_capture_mode capture_mode;
    int fmt;
    int width;
    int height;
    int frame_size;
    union {
        struct {
            int capture_buffers_current_index;
            struct frame_reader_buffers* capture_buffers;
        };
        void* capture_buffer;
    };
    size_t capture_buffers_length;
    int texture_format;
    void* decode_buffer;

};

struct frame_reader* reader_new(int fd, enum supported_capture_mode mode, enum supported_capture_format fmt, int width, int height, int frame_size) {
    struct frame_reader* fr = calloc(1, sizeof(struct frame_reader));
    fr->fd = fd;
    fr->capture_mode = mode;

    fr->fmt = fmt;
    fr->width = width;
    fr->height = height;
    fr->frame_size = frame_size;

    switch (fmt) {
        case capture_format_RGB24:
            fr->texture_format = GL_RGB;
            break;
        case capture_format_YUYV:
        case capture_format_MJPEG:
        case capture_format_NV12:
            fr->texture_format = GL_RGB;
            fr->decode_buffer = malloc(sizeof(char) * width *height * 3);
            break;
    }

    switch (mode) {
        case capture_mode_read:
            fr->capture_buffer = malloc(sizeof(char) * frame_size);
            fr->capture_buffers_length = 1;
            break;

        case capture_mode_mmap:
            fr->capture_buffers_current_index = -1;
            fr->capture_buffers_length = 2;
            struct v4l2_requestbuffers requestbuffers = {0};
            requestbuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            requestbuffers.memory = V4L2_MEMORY_MMAP;
            requestbuffers.count = fr->capture_buffers_length;
            int request_status = ioctl(fr->fd, VIDIOC_REQBUFS, &requestbuffers);
            // TODO handle error
            (void) request_status;
            if(requestbuffers.count < fr->capture_buffers_length) {
                // TODO handle error
            }

            fr->capture_buffers = calloc(fr->capture_buffers_length, sizeof(struct frame_reader_buffers));

            for (size_t i = 0; i < fr->capture_buffers_length; ++i) {
                struct v4l2_buffer buf = {0};

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = i;

                int query_status = ioctl(fr->fd, VIDIOC_QUERYBUF, &buf);
                // TODO error handling
                (void) query_status;

                fr->capture_buffers[i].ptr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fr->fd, buf.m.offset);
                fr->capture_buffers[i].length = buf.length;

                if (MAP_FAILED == fr->capture_buffers[i].ptr) {
                    // TODO error handling
                }
            }
            break;
    }

    return fr;
}

void reader_start(struct frame_reader* reader) {

    switch (reader->capture_mode) {
        case capture_mode_read:
            return;
        case capture_mode_mmap:
            // but all the buffers in the queue
            for (size_t i = 0; i < reader->capture_buffers_length; ++i) {
                struct v4l2_buffer buf = {0};
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
                ioctl(reader->fd, VIDIOC_QBUF, &buf);
            }
            // start capturing
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            int stream_on_status = ioctl(reader->fd, VIDIOC_STREAMON, &type);
            // TODO error handling
            (void) stream_on_status;
            break;
    }
}


void* reader_read_raw(struct frame_reader* reader) {
    switch (reader->capture_mode) {
        case capture_mode_read:
            int bytes = read(reader->fd, reader->capture_buffer, reader->frame_size);
            if (bytes == -1) {
                switch (errno) {
                    case EAGAIN:
                        // skip frame read, this time preset old data again.
                        return reader->capture_buffer;
                    default:
                        return NULL;
                }
            }

            if (bytes != reader->frame_size) {
                return NULL;
            }

            return reader->capture_buffer;
        case capture_mode_mmap:
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            // queue last used buffer if one was in use
            if (reader->capture_buffers_current_index != -1) {
                buf.index = reader->capture_buffers_current_index;
                if (-1 == ioctl(reader->fd, VIDIOC_QBUF, &buf)) {
                    // TODO error handling
                    return NULL;
                }
                reader->capture_buffers_current_index = -1;
            }

            // dequeue buffer
            if (-1 == ioctl(reader->fd, VIDIOC_DQBUF, &buf)) {
                // TODO error handling
                return NULL;
            }
            reader->capture_buffers_current_index = buf.index;
            return reader->capture_buffers[reader->capture_buffers_current_index].ptr;
    }
    return NULL;
}

#define cc(v) ((v < 0) ? 0 : (255 < v) ? 255 : v)

void reader_decode_nv12(unsigned char* in, unsigned char* out, size_t width, size_t height) {

    // https://learn.microsoft.com/en-gb/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering#nv12
    // https://fourcc.org/fccyvrgb.php
    // https://gist.github.com/dmitriykovalev/980f4bcf68ac4667e89d4d989de21835
    size_t i, j = 0;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {

            int index = i * width + j;
            int offset = height * width, I = i/2, J = j/2, W = width/2;

            int y = in[index] - 16;
            int u = in[offset + (I*W+J)*2] - 128;
            int v = in[offset + (I*W+J)*2+1] - 128;

            out[index * 3 + 3] = cc(1.164 * y + 2.018 * u);
            out[index * 3 + 1] = cc(1.164 * y - 0.813 * v - 0.391 * u);
            out[index * 3 + 0] = cc(1.164 * y + 1.596 * v);
        }
    }
}

void reader_decode_yuyv(unsigned char* in, unsigned char* out, size_t width, size_t height) {

    // https://fourcc.org/fccyvrgb.php
    size_t i, j = 0;
    int index, y0, u, y1, v;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j += 2) {
            index = i * width + j;
            y0 = in[index * 2 + 0];
            u = in[index * 2 + 1];
            y1 = in[index * 2 + 2];
            v = in[index * 2 + 3];

            out[index * 3 + 0] = cc(1.164*(y0 - 16) + 1.596*(v - 128));
            out[index * 3 + 1] = cc(1.164*(y0 - 16) - 0.813*(v - 128) - 0.391*(u - 128));
            out[index * 3 + 2] = cc(1.164*(y0 - 16) + 2.018*(u - 128));

            out[index * 3 + 3] = cc(1.164*(y1 - 16) + 1.596*(v - 128));
            out[index * 3 + 4] = cc(1.164*(y1 - 16) - 0.813*(v - 128) - 0.391*(u - 128));
            out[index * 3 + 5] = cc(1.164*(y1 - 16) + 2.018*(u - 128));
        }
    }
}

void reader_decode_mjpeg(unsigned char* in, unsigned char* out, size_t input_size, size_t width, size_t height) {

    int w, h, n_channels;
    unsigned char* image = stbi_load_from_memory(in, input_size, &w, &h, &n_channels, 3);

    memcpy(out, image, width*height*3);

    stbi_image_free(image);
}


void* reader_read_decode_rgb(struct frame_reader* reader) {
    enum supported_capture_format format = reader->fmt;
    switch (format) {
        case capture_format_RGB24:
            return reader_read_raw(reader);
        case capture_format_YUYV:
            //return reader_read_raw(reader);
            reader_decode_yuyv(reader_read_raw(reader), reader->decode_buffer, reader->width, reader->height);
            return reader->decode_buffer;
        case capture_format_MJPEG:
            reader_decode_mjpeg(reader_read_raw(reader), reader->decode_buffer, reader->frame_size, reader->width, reader->height);
            return reader->decode_buffer;
        case capture_format_NV12:
            reader_decode_nv12(reader_read_raw(reader), reader->decode_buffer, reader->width, reader->height);
            return reader->decode_buffer;
    }
    return NULL;
}

void reader_postprocess(struct frame_reader* reader) {
    (void) reader;
}

void reader_stop(struct frame_reader* reader) {
    switch (reader->capture_mode) {
        case capture_mode_read:
            return;
        case capture_mode_mmap:
            // dequeue last used buffer
            struct v4l2_buffer buf = {0};
            if (reader->capture_buffers_current_index != -1) {
                buf.index = reader->capture_buffers_current_index;
                if (-1 == ioctl(reader->fd, VIDIOC_QBUF, &buf)) {
                    // TODO error handling
                }
                reader->capture_buffers_current_index = -1;
            }

            // stop capturing
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            int streamoff_status = ioctl(reader->fd, VIDIOC_STREAMOFF, &type);
            // TODO error handling
            (void) streamoff_status;
            break;
    }
}

void reader_destroy(struct frame_reader* reader) {

    switch (reader->fmt) {
        case capture_format_RGB24:
            break;
        case capture_format_YUYV:
        case capture_format_MJPEG:
            free(reader->decode_buffer);
            break;
    }

    switch (reader->capture_mode) {
        case capture_mode_read:
            free(reader->capture_buffer);
            break;
        case capture_mode_mmap:

            // remove all buffers from the queue
            for (size_t i = 0; i < reader->capture_buffers_length; ++i) {
                int status = munmap(reader->capture_buffers[i].ptr, reader->capture_buffers[i].length);
                (void) status;
                // TODO error handling
            }

            free(reader->capture_buffers);
            break;
    }

    free(reader);
}


