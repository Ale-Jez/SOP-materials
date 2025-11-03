
Taken from https://github.com/adamgracikowski/Operating-Systems/tree/main/SOP2/lab03


``` csharp
#include <pthread.h>

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_timedrdlock(pthread_rwlock_t* rwlock, const struct timespec* abstime);
/* Opis:
    - applies a read lock to the read-write lock referenced by rwlock. The calling thread
      acquires the read lock if a writer does not hold the lock and there are no writers
      blocked on the lock.
    - if a signal is delivered to a thread waiting for a read-write lock for reading,
      upon return from the signal handler the thread resumes waiting for the read-write
      lock for reading as if it was not interrupted.
    - on success returns 0, otherwise a positive number to indicate the error.
    - the timeout in pthread_rwlock_timedrdlock shall be based on the CLOCK_REALTIME clock.
*/

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_timedwrlock(pthread_rwlock_t* rwlock, const struct timespec* abstime);
/* Opis:
    - applies a write lock to the read-write lock referenced by rwlock.
    - the calling thread shall acquire the write lock if no thread (reader or writer)
      holds the read-write lock rwlock.
    - otherwise, if another thread holds the read-write lock rwlock, the calling thread
      shall block until it can acquire the lock.
*/

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
/* Opis:
    - releases a lock held on the read-write lock object referenced by rwlock.
*/

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* attr, int* pshared);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared);

```