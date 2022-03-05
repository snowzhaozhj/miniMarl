#ifndef MINIMARL_INCLUDE_MARL_SANITIZER_HPP_
#define MINIMARL_INCLUDE_MARL_SANITIZER_HPP_

// Define ADDRESS_SANITIZER_ENABLED to 1 if the project was built with the
// address sanitizer enabled (-fsanitize=address).
#if defined(__SANITIZE_ADDRESS__)
#define ADDRESS_SANITIZER_ENABLED 1
#else  // defined(__SANITIZE_ADDRESS__)
#if defined(__clang__)
#if __has_feature(address_sanitizer)
#define ADDRESS_SANITIZER_ENABLED 1
#endif  // __has_feature(address_sanitizer)
#endif  // defined(__clang__)
#endif  // defined(__SANITIZE_ADDRESS__)

// ADDRESS_SANITIZER_ONLY(X) resolves to X if ADDRESS_SANITIZER_ENABLED is
// defined to a non-zero value, otherwise ADDRESS_SANITIZER_ONLY() is stripped
// by the preprocessor.
#if ADDRESS_SANITIZER_ENABLED
#define ADDRESS_SANITIZER_ONLY(x) x
#else
#define ADDRESS_SANITIZER_ONLY(x)
#endif  // ADDRESS_SANITIZER_ENABLED

// Define MEMORY_SANITIZER_ENABLED to 1 if the project was built with the memory
// sanitizer enabled (-fsanitize=memory).
#if defined(__SANITIZE_MEMORY__)
#define MEMORY_SANITIZER_ENABLED 1
#else  // defined(__SANITIZE_MEMORY__)
#if defined(__clang__)
#if __has_feature(memory_sanitizer)
#define MEMORY_SANITIZER_ENABLED 1
#endif  // __has_feature(memory_sanitizer)
#endif  // defined(__clang__)
#endif  // defined(__SANITIZE_MEMORY__)

// MEMORY_SANITIZER_ONLY(X) resolves to X if MEMORY_SANITIZER_ENABLED is defined
// to a non-zero value, otherwise MEMORY_SANITIZER_ONLY() is stripped by the
// preprocessor.
#if MEMORY_SANITIZER_ENABLED
#define MEMORY_SANITIZER_ONLY(x) x
#else
#define MEMORY_SANITIZER_ONLY(x)
#endif  // MEMORY_SANITIZER_ENABLED

// Define THREAD_SANITIZER_ENABLED to 1 if the project was built with the thread
// sanitizer enabled (-fsanitize=thread).
#if defined(__SANITIZE_THREAD__)
#define THREAD_SANITIZER_ENABLED 1
#else  // defined(__SANITIZE_THREAD__)
#if defined(__clang__)
#if __has_feature(thread_sanitizer)
#define THREAD_SANITIZER_ENABLED 1
#endif  // __has_feature(thread_sanitizer)
#endif  // defined(__clang__)
#endif  // defined(__SANITIZE_THREAD__)

// THREAD_SANITIZER_ONLY(X) resolves to X if THREAD_SANITIZER_ENABLED is defined
// to a non-zero value, otherwise THREAD_SANITIZER_ONLY() is stripped by the
// preprocessor.
#if THREAD_SANITIZER_ENABLED
#define THREAD_SANITIZER_ONLY(x) x
#else
#define THREAD_SANITIZER_ONLY(x)
#endif  // THREAD_SANITIZER_ENABLED

// The CLANG_NO_SANITIZE_MEMORY macro suppresses MemorySanitizer checks for
// use-of-uninitialized-data. It can be used to decorate functions with known
// false positives.
#ifdef __clang__
#define CLANG_NO_SANITIZE_MEMORY __attribute__((no_sanitize_memory))
#else
#define CLANG_NO_SANITIZE_MEMORY
#endif

#endif //MINIMARL_INCLUDE_MARL_SANITIZER_HPP_
