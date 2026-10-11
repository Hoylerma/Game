#pragma once

// Kleiner Thread-Pool für die Mehrkernberechnung (seit M4, docs/technik/bevoelkerung.md, Abschnitt 5).
//
// run(count, chunk, fn) teilt die Indizes [0, count) in Pakete fester Größe und ruft fn(begin, end)
// für jedes Paket genau einmal auf. Die Threads holen sich Pakete über einen atomaren Zähler; der
// aufrufende Thread rechnet mit. run kehrt erst zurück, wenn alle Pakete fertig sind.
//
// Welcher Thread welches Paket rechnet, ist nicht festgelegt. Determinismus entsteht beim
// Aufrufer: Jedes Paket schreibt nur eigene Daten und liest nur Daten, die während run niemand
// schreibt. Dann ist das Ergebnis unabhängig von der Zahl der Threads.

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace uhrwerk {

class ThreadPool {
public:
    using ChunkFn = std::function<void(std::size_t begin, std::size_t end)>;

    /// threads: Gesamtzahl der rechnenden Threads einschließlich des aufrufenden;
    /// 0 = alle Kerne (resolve_threads).
    explicit ThreadPool(unsigned threads = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /// Zahl der rechnenden Threads einschließlich des aufrufenden (mindestens 1).
    unsigned threads() const { return static_cast<unsigned>(workers_.size()) + 1; }

    /// Ruft fn für alle Pakete von [0, count) auf (chunk 0 gilt als 1). Mit einem Thread oder
    /// nur einem Paket läuft alles im aufrufenden Thread, Pakete aufsteigend. Wirft fn, wird die
    /// erste Ausnahme nach dem Ende aller Pakete weitergeworfen.
    void run(std::size_t count, std::size_t chunk, const ChunkFn& fn);

    /// 0 → std::thread::hardware_concurrency() (mindestens 1), sonst der Wert selbst.
    static unsigned resolve_threads(unsigned requested);

private:
    void worker_loop();
    void work();

    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable done_;
    std::uint64_t generation_ = 0; // zählt Aufträge; geschützt durch mutex_
    bool stop_ = false;
    unsigned active_ = 0; // Arbeiter, die den aktuellen Auftrag noch nicht beendet haben

    // Aktueller Auftrag; wird unter mutex_ gesetzt, bevor generation_ steigt.
    const ChunkFn* fn_ = nullptr;
    std::size_t count_ = 0;
    std::size_t chunk_ = 1;
    std::atomic<std::size_t> next_chunk_{0};
    std::exception_ptr error_;
};

} // namespace uhrwerk
