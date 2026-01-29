#ifndef WORKERUTILS_HPP
#define WORKERUTILS_HPP

#include <semaphore>

#include <sys/time.h>

// Struct used for signaling thread startup completion
// Struct used for signaling thread startup completion
struct StartHelper
{
    int encChn;
    int MipiMode = 0;
    uint32_t Time_interval = 2000;
    std::binary_semaphore has_started{0};
};

namespace WorkerUtils {

unsigned long long tDiffInMs(struct timeval *startTime);

} // namespace WorkerUtils

#endif // WORKERUTILS_HPP
