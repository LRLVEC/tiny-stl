#pragma once
#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <thread>
#include <vector>

template <typename T> void waitAll(T&& futures)
{
	for (auto& f : futures)
	{
		f.get();
	}
}
struct ThreadPool
{
	size_t mNumThreads = 0;
	std::vector<std::thread> mThreads;

	std::deque<std::function<void()>> mTaskQueue;
	std::mutex mTaskQueueMutex;
	std::condition_variable mWorkerCondition;

	std::atomic<size_t> mNumTasksInSystem;
	std::mutex mSystemBusyMutex;
	std::condition_variable mSystemBusyCondition;


	ThreadPool() : ThreadPool{ std::thread::hardware_concurrency() } {}
	ThreadPool(size_t maxNumThreads, bool force = false)
	{
		if (!force)
		{
			maxNumThreads = std::min((size_t)std::thread::hardware_concurrency(), maxNumThreads);
		}
		startThreads(maxNumThreads);
		mNumTasksInSystem.store(0);
	}
	virtual ~ThreadPool()
	{
		shutdownThreads(mThreads.size());
	}

	template <class F>
	auto enqueueTask(F&& f, bool highPriority = false) -> std::future<std::result_of_t <F()>>
	{
		using return_type = std::result_of_t<F()>;
		++mNumTasksInSystem;
		auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
		auto res = task->get_future();

		{
			std::lock_guard<std::mutex> lock{ mTaskQueueMutex };

			if (highPriority)
			{
				mTaskQueue.emplace_front([task]() { (*task)(); });
			}
			else
			{
				mTaskQueue.emplace_back([task]() { (*task)(); });
			}
		}

		mWorkerCondition.notify_one();
		return res;
	}

	void startThreads(size_t num)
	{
		mNumThreads += num;
		for (size_t i = mThreads.size(); i < mNumThreads; ++i)
		{
			mThreads.emplace_back([this, i]
				{
					while (true)
					{
						std::unique_lock<std::mutex> lock{ mTaskQueueMutex };

						// look for a work item
						while (i < mNumThreads && mTaskQueue.empty())
						{
							// if there are none wait for notification
							mWorkerCondition.wait(lock);
						}

						if (i >= mNumThreads)
						{
							break;
						}

						std::function<void()> task{ move(mTaskQueue.front()) };
						mTaskQueue.pop_front();

						// Unlock the lock, so we can process the task without blocking other threads
						lock.unlock();

						task();

						mNumTasksInSystem--;

						{
							std::unique_lock<std::mutex> localLock{ mSystemBusyMutex };

							if (mNumTasksInSystem == 0)
							{
								mSystemBusyCondition.notify_all();
							}
						}
					}
				});
		}
	}
	void shutdownThreads(size_t num)
	{
		auto numToClose = std::min(num, mNumThreads);
		{
			std::lock_guard<std::mutex> lock{ mTaskQueueMutex };
			mNumThreads -= numToClose;
		}

		// Wake up all the threads to have them quit
		mWorkerCondition.notify_all();
		for (auto i = 0u; i < numToClose; ++i)
		{
			mThreads.back().join();
			mThreads.pop_back();
		}
	}

	size_t numTasksInSystem() const
	{
		return mNumTasksInSystem;
	}

	void waitUntilFinished()
	{
		std::unique_lock<std::mutex> lock{ mSystemBusyMutex };

		if (mNumTasksInSystem == 0)
		{
			return;
		}

		mSystemBusyCondition.wait(lock);
	}
	void waitUntilFinishedFor(const std::chrono::microseconds Duration)
	{
		std::unique_lock<std::mutex> lock{ mSystemBusyMutex };

		if (mNumTasksInSystem == 0)
		{
			return;
		}

		mSystemBusyCondition.wait_for(lock, Duration);
	}
	void flushQueue()
	{
		std::lock_guard<std::mutex> lock{ mTaskQueueMutex };

		mNumTasksInSystem -= mTaskQueue.size();
		mTaskQueue.clear();
	}

	template <typename Int, typename F>
	void parallelForAsync(Int start, Int end, F body, std::vector<std::future<void>>& futures)
	{
		Int localNumThreads = (Int)mNumThreads;

		Int range = end - start;
		Int chunk = (range / localNumThreads) + 1;

		for (Int i = 0; i < localNumThreads; ++i)
		{
			futures.emplace_back(enqueueTask([i, chunk, start, end, body]
				{
					Int innerStart = start + i * chunk;
					Int innerEnd = std::min(end, start + (i + 1) * chunk);
					for (Int j = innerStart; j < innerEnd; ++j)
					{
						body(j);
					}
				}));
		}
	}

	template <typename Int, typename F>
	std::vector<std::future<void>> parallelForAsync(Int start, Int end, F body)
	{
		std::vector<std::future<void>> futures;
		parallelForAsync(start, end, body, futures);
		return futures;
	}

	template <typename Int, typename F>
	void parallelFor(Int start, Int end, F body)
	{
		waitAll(parallelForAsync(start, end, body));
	}

};
