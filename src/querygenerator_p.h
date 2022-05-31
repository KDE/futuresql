#pragma once

#include <QFutureInterface>
#include <QFutureWatcher>

#include <type_traits>
#include <memory>

///
/// Applies a function to a the contained value of a QFuture once it finishes.
/// @returns a QFuture that finishes once the future given as first argument finished
///
template <typename T, typename Func>
QFuture<std::invoke_result_t<Func, T>> mapFuture(const QFuture<T> &future, Func mapFunction) {
    using ReturnType = std::invoke_result_t<Func, T>;
    auto watcher = std::make_shared<QFutureWatcher<T>>();
    watcher->setFuture(future);

    auto interface = std::make_shared<QFutureInterface<ReturnType>>();
    QObject::connect(watcher.get(), &QFutureWatcherBase::finished, watcher.get(), [interface, watcher, mapFunction] {
        auto result = watcher->result();
        auto mapped = mapFunction(std::move(result));
        interface->reportResult(mapped);
        interface->reportFinished();
    });

    return interface->future();
}
