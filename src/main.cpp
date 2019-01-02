// Copyright (c) 2016-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <glib.h>
#include <string>
#include <sys/signalfd.h>

#include "logging.h"
#include "systemproperty/systempropertyservice.h"
#include "video/videoservice.h"
#include <val_api.h>

PmLogContext logContext;

static const char *const logContextName = "videooutputd";
static const char *const logPrefix      = "[videooutputd] ";
static const std::string busName        = "com.webos.service.videooutput";

static gboolean option_version = FALSE;
static GMainLoop *mainLoop     = nullptr;
static bool terminated         = false;

static GOptionEntry options[] = {
    {"version", 'v', 0, G_OPTION_ARG_NONE, &option_version, "Show version information and exit", ""},
    {NULL, ' ', 0, G_OPTION_ARG_NONE, NULL, NULL, NULL},
};

static void lunaBusDisconnected(LSHandle *sh, void *user_data)
{
    terminated = 1;
    g_main_loop_quit(mainLoop);
}

static gboolean signal_handler(GIOChannel *channel, GIOCondition cond, gpointer user_data)
{
    struct signalfd_siginfo si;
    ssize_t result;
    int fd;

    if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
        return FALSE;
    }

    fd = g_io_channel_unix_get_fd(channel);

    result = read(fd, &si, sizeof(si));

    if (result != sizeof(si)) {
        return FALSE;
    }

    switch (si.ssi_signo) {
    case SIGUSR1:
        /* This will exit the service without executing the shutdown steps.
         * Use to simulate various test situations. */
        exit(EXIT_FAILURE);
        break;

    case SIGINT:
    case SIGTERM:
        if (terminated == 0) {
            g_main_loop_quit(mainLoop);
        }

        terminated = 1;
        break;

    default:
        break;
    }

    return TRUE;
}

/**
 * Setup handing of signals.
 * @return geventSource to remove to remove the handlers.
 */
static guint setup_signalfd()
{
    GIOChannel *channel;
    guint source;
    sigset_t mask;
    int fd;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        std::cerr << "Failed to set signal mask";
        LOG_ERROR(MSGID_SIGNAL_HANDLER_ERROR, 0, "Failed to set signal mask");
        return 0;
    }

    fd = signalfd(-1, &mask, 0);

    if (fd < 0) {
        std::cerr << "Failed to create signal descriptor";
        LOG_ERROR(MSGID_SIGNAL_HANDLER_ERROR, 0, "Failed to create signal descriptor");
        return 0;
    }

    channel = g_io_channel_unix_new(fd);

    g_io_channel_set_close_on_unref(channel, TRUE);
    g_io_channel_set_encoding(channel, NULL, NULL);
    g_io_channel_set_buffered(channel, FALSE);

    source = g_io_add_watch(channel, static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL),
                            signal_handler, NULL);

    g_io_channel_unref(channel);

    return source;
}

int main(int argc, char **argv)
{
    GOptionContext *context;
    GError *err = NULL;

    context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, options, NULL);

    if (g_option_context_parse(context, &argc, &argv, &err) == FALSE) {
        if (err != NULL) {
            std::cerr << logPrefix << err->message << std::endl;
            g_error_free(err);
        } else
            std::cerr << logPrefix << "An unknown error occurred" << std::endl;
        exit(EXIT_FAILURE);
    }

    g_option_context_free(context);

    // TODO::Where to get version from
    /*if (option_version) {
            std::cout << VERSION << std::endl;
            exit(EXIT_SUCCESS);
    }
*/

    PmLogErr error = PmLogGetContext(logContextName, &logContext);
    if (error != kPmLogErr_None) {
        std::cerr << logPrefix << "Failed to setup up log context " << logContextName << std::endl;
        exit(EXIT_FAILURE);
    }

    mainLoop     = g_main_loop_new(NULL, FALSE);
    guint signal = setup_signalfd();

    // Initiate val object
    VAL *val = VAL::getInstance();
    try {
        if (!val->initialize()) {
            LOG_ERROR(MSGID_HAL_INIT_ERROR, 0,
                      "VAL initialization failed! Service is still starting, but some functionality might not work.");
        }

        LS::Handle serviceHandle{busName.c_str()};

        // Initialize categories
        VideoService video(serviceHandle);
        SystemPropertyService systemproperties(serviceHandle, video);
#if !USE_RPI_RESOURCE
        PictureSettings pictureSettings(serviceHandle, video);
#endif
        AspectRatioSetting arcSetting(serviceHandle, video);

        serviceHandle.attachToLoop(mainLoop);
        serviceHandle.setDisconnectHandler(lunaBusDisconnected, nullptr);
        g_main_loop_run(mainLoop);
    } catch (const std::exception &e) {
        std::cerr << logPrefix << "Caught exception: '" << e.what() << "' exiting" << std::endl;
        LOG_ERROR(MSGID_UNEXPECTED_EXCEPTION, 0, "%s, exiting.", e.what());
        exit(EXIT_FAILURE);
    } catch (...) {
        std::cerr << logPrefix << "Caught exception, exiting" << std::endl;
        LOG_ERROR(MSGID_UNEXPECTED_EXCEPTION, 0, "Exiting ");
        exit(EXIT_FAILURE);
    }

    LOG_INFO(MSGID_TERMINATING, 0, "Terminating");

    g_source_remove(signal);
    g_main_loop_unref(mainLoop);

    if (!val->deinitialize()) {
        LOG_ERROR(MSGID_HAL_DEINIT_ERROR, 0, "VAL deinitialization error. See logs for details.");
    }

    return EXIT_SUCCESS;
}
