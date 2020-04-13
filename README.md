# v1util: An opinionated pragmatic's toolbelt for C++ development

This repository is a collection of tools that make up a solid foundation to
develop C++ software in a more straight-forward, less painful way.

## Requirements

You need a C++17 compiler. The framework supports:
- MSVC 2019
- clang 9
- gcc 8

The toolbelt is supported on the following platforms:
- FreeBSD
- Linux
- Windows

## How to build

On Unix:
```
mkdir build-cmake; cd build-cmake && cmake -DCMAKE_UNITY_BUILD=1 ../ && make
```

On Windows:
* download [FASTBuild](https://www.fastbuild.org/)
* customise `fbuild-infrastructure/VS2019.bff`
* customise `fbuild-infrastructure/Windows10SDK.bff`
* call `FBuild`
* IDE files are in `build/ide`, output in `build/out`

## Design decisions

- move constructors and move assignments don't throw
- fast compilation speed is mandatory; this includes:
  - embrace unity builds
  - avoid non-`explicit` single-argument constructors
  - prefer `if constexpr` over `std::enable_if_t`
  - avoid template trickery that relies on recursion
- I don't want to pay for things I don't need, like:
  - support throwing excepions from all places theoretically possible
  - bad STL APIs, e.g. `std::unordered_map` bucket interface and all its
    implications
- make use of tailor-made custom containers, e.g. `Delegate`, `ArrayView`

### Background

Don't get me wrong, the people designing the STL specification and the people
actually implementing it  are doing an incredibly hard job. For example, to get
the intersection of move semantics, exceptions and backwards compatibility right
is a difficult task!
Nonetheless, sometimes things sneak into the standard that turn out to be
not so sood. The STL algorithms are fine. Yet, when it comes to containers, you
might want to roll your own, including a convenience layer.
I was pointed at `std::lerp` for an example of how difficult something seemingly
simple can be. It is up to you to decide what level of safety and complexity you
need. I am under the belief of knowning what I do, so I would loosen my safety
belt a bit to gain speed and simplicity. To be clear: This depends on the
context. My decisions would be far different if I would build software for an
atomic power plant! This repository is only used in projects with no risk to
harm anyone if such decisions turn out to be wrong.

Also, please keep in mind that this repository presents code that I write in my
spare time. The style and compromises I choose here are vastly different from
production code that I write at work. There, robustness, test coverage,
maintaineability, safety, readability and being self-explainatory are more
important than in my private projects.

## Notable parts

* `StreamingPeakFinder`: Extract peaks from a stream<br/>
  Implements a simple greedy algorithm that extracts peaks that dominate (i.e.
  are bigger in size or the same size and the first) other peaks within a
  certain window size. Plateaus of same-valued samples are aggreated into the
  middle element. It is similar to the windowed local maxima, yet still
  different.

## Building blocks
* `Delegate`, `Function`: Non-owning and owning references to a Callable<br/>
  `Function` is functionally equivalent to `std::function`, but is often smaller
  and has the potential to support a kind of equality comparison. This is more
  of an exercise to demonstrate how easy it is to create a small type-erased
  callable reference, but how difficult it is to get all the small details
  right.
* `ArrayView`, `Span`: Read-only and read-write non-owning references to
  contiguous memory<br/>
  They are light-weight, convenient implementations of `std::span`, restricted
  to dynamic extends. At this point, I'm still a firm believer in `const` and
  non-`const` access to the underlying memory region being very different cases,
  mandating the use of different containers. Yet, for simplicity in generic
  code, unifying them could be beneficial. This need hasn't arised so far on my
  side.
* `TscStamp`, `TscDiff`: High-precision time counter point in time and time
  difference<br/>
  Sure, `std::chrono` will give you the same. I just think it's overcomplicated.
  The generic unit conversion business makes not sense for times when your
  clocks have a very defined native resolution. This has been exploited by many
  file systems that store time stamps only with a resolution of 100 ns. That is
  a lot less generic that `std::chrono`, but it suffices for my use cases.
* `tracing.hpp`: A C++ interface to [uucidl](https://github.com/uucidl/)'s
  excellent tracing library<br/>


# Licensing

This library is released under the terms in `LICENSE`, the FreeBSD license.

## Third-Party dependencies
The third-party dependencies of this library are licensed under their own terms:

<!-- V1LIC-BEGIN-LICENSES -->
<!-- V1LIC-BEGIN-LICENSE path=fbuild-infrastructure hash=A6E2D26637D574A1 -->
* [FASTBuild](https://github.com/fastbuild/fastbuild/): A high-performance build system<br/>
  FASTBuild license, license file: <!-- V1LIC-LICENSE-FILE-PATH --> `fbuild-infrastructure/LICENSE.TXT` <br/>
  Some `.bff` files origin from FASTBuild.
<!-- V1LIC-BEGIN-LICENSE path=third-party/doctest hash=D9F0EF78E469DF3E -->
* [doctest](https://github.com/onqtam/doctest/): A fast C++ testing framework<br/>
  MIT-licensed; license file: <!-- V1LIC-LICENSE-FILE-PATH --> `third-party/doctest/LICENSE.txt` <br/>
<!-- V1LIC-BEGIN-LICENSE path=third-party/nano-signal-slot hash=D9F0EF78E469DF3E -->
* [nano-signal-slot](https://github.com/NoAvailableAlias/nano-signal-slot/): Signals and Slots<br/>
  MIT-licensed; license file: <!-- V1LIC-LICENSE-FILE-PATH --> `third-party/nano-signal-slot/LICENSE` <br/>
  Please look into alternatives. Semantics are not sane with regards to reentrancy:
  Signals get delivered to disconnected slots or disconnecting during signal delivery deadlocks.
<!-- V1LIC-BEGIN-LICENSE path=third-party/sltbench hash=B8CEB1ED58332868 -->
* [sltbench](https://github.com/ivafanas/sltbench/): Practical, stable and fast performance testing framework<br/>
  Apache 2.0-licensed; license file: <!-- V1LIC-LICENSE-FILE-PATH --> `third-party/sltbench/LICENSE` <br/>
<!-- V1LIC-BEGIN-LICENSE path=third-party/uu.spdr hash=0158F023BA98492E -->
* [uu.spdr](https://github.com/uucidl/uu.spdr/): Tracing framework that interfaces with `chrome://tracing`<br/>
  MIT-licensed; license file: <!-- V1LIC-LICENSE-FILE-PATH --> `third-party/uu.spdr/LICENSE` <br/>
<!-- V1LIC-BEGIN-LICENSE path=third-party/uu.spdr/deps/libatomic_ops-7.6.6/doc hash=B4486F4C362531A5 -->
* [libatomic_ops](http://www.hboehm.info/gc/gc_source/libatomic_ops-7.6.6.tar.gz): Portable C atomic operations library<br/>
  MIT-alike-licensed, license file: <!-- V1LIC-LICENSE-FILE-PATH --> `third-party/uu.spdr/deps/libatomic_ops-7.6.6/doc/LICENSING.txt` <br/>
  Only used by `uu.spdr`. GPL parts are not compiled.
<!-- V1LIC-END-LICENSES -->
