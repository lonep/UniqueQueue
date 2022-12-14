
#include <condition_variable>
#include <mutex>
#include <thread>
#include <iostream>
#include <queue>
#include <memory>
#include <set>
#include <ctime>

//ЗАДАНИЕ
//Напишите функцию перенаправляющие данные из одного анал в другой, 
//в случае если данных в канала нет более n миллисекунд отправлять в канал для записи значение : timeout

template <typename T>
class threadsafe_queue
{
private:
	
	mutable std::mutex mut;
	std::queue<T> data_queue;
	std::condition_variable data_cond;
	std::set<T> unique_data;

public:
	explicit threadsafe_queue() {}; 
	
	explicit threadsafe_queue(const threadsafe_queue& other) 
	{
		
		std::lock_guard<std::mutex> lock(mut);
		data_queue = other.data_queue;
		unique_data = other.unique_data;
	};
	
	
	
	void push(T value) 
	{
		//блокируем, кладём, разблокируем
		std::lock_guard<std::mutex> lock(mut);
		if (unique_data.find(value) == unique_data.end()) //добавляем ТОЛЬКО уникальные элементы
		{
			data_queue.push(value);
			unique_data.emplace(data_queue.back());
		}		
		data_cond.notify_one();
	};
	
	
	void wait_and_pop(T& value) 
	{
		//заблокируется, пока не получит уведомление, что в поток что-то поместили
		std::unique_lock<std::mutex> lock(mut);
		//блокирует поток
		
		std::clock_t c_start = std::clock(); 
		auto t_start = std::chrono::high_resolution_clock::now(); //Стартуем таймер ожидания.

		data_cond.wait(lock, !data_queue.empty());

		std::clock_t c_end = std::clock();
		auto t_end = std::chrono::high_resolution_clock::now();
		
		if (std::chrono::duration<double, std::milli>(t_end - t_start).count() > 3000)
			std::cout << "TIMEOUT"; //Если ждем больше 3 секунд, тогда кидаем timeout.

		value = data_queue.front();
		std::set<int>::iterator it = unique_data.find(value);
		unique_data.erase(it);
		data_queue.pop();
	};

	std::shared_ptr<const T> wait_and_pop()
	{
		std::unique_lock<std::mutex> lock(mut);
		data_cond.wait(lock, [this]
			{
				return !data_queue.empty();
			});

		//умный указатель на первый элемент очереди
		std::shared_ptr<const T> res(std::make_shared<const T>(data_queue.front()));
		std::set<int>::iterator it = unique_data.find(*res);
		unique_data.erase(it);
		data_queue.pop();
		return res;
	};

	
	bool try_pop(T& value) //не будет ждать - посмотрит в очереди, заблокирует, удалит
	{
		std::lock_guard<std::mutex> lock(mut);
		if (data_queue.empty())
		{
			return false;
		}
		value = data_queue.front();
		std::set<int>::iterator it = unique_data.find(value);
		unique_data.erase(it);
		data_queue.pop();
		return true;
	};

	std::shared_ptr<const T> try_pop()
	{
		std::lock_guard<std::mutex> lock(mut);
		if (data_queue.empty())
		{
			return nullptr;
		}
		std::shared_ptr<const T> res(std::make_shared<const T>(data_queue.front()));
		std::set<int>::iterator it = unique_data.find(*res);
		unique_data.erase(it);
		data_queue.pop();
		return res;
	};

	bool empty() const 
	{
		std::lock_guard<std::mutex> lock(mut);
		return data_queue.empty();
	};

	void printAndCrearData()
	{	
		while (!data_queue.empty())
		{
			printf("%i \n", *wait_and_pop());
			
		}

	}

};


void pushTest(threadsafe_queue <int>& queue) {
	queue.push(100);
}

void tryPopTest(threadsafe_queue <int>& queue) {
	std::cout << "Try_pop complete - " << *queue.try_pop() << "\n";
}

int main()
{
	//Тест 1.
	{	
		std::cout << "TEST 1 (PUSH TEST) STARTED" << "\n";

		threadsafe_queue<int> ourQueue{};
		std::thread thr(pushTest, std::ref(ourQueue));
		thr.detach(); 
		ourQueue.push(200); 
		ourQueue.printAndCrearData(); //Выведет 200 из-за того, что один поток впал в ожидание и на момент вызова метода не закончил push()

		std::cout << "TEST 1 (PUSH TEST) ENDED" << "\n";
	}

	std::cout << "-------------------------------------------------" << "\n";

	//Тест 2.
	{
		std::cout << "TEST 2 (PUSH TEST SYNC) STARTED" << "\n";

		threadsafe_queue<int> ourQueue{};
		std::thread thr(pushTest, std::ref(ourQueue));
		thr.detach();
		ourQueue.push(200);

		std::this_thread::sleep_for(std::chrono::microseconds(200));
		ourQueue.printAndCrearData(); //Выведет 200, 100 т.к. с помощью ожидания оба поток закончили выполнение push()

		std::cout << "TEST 2 (PUSH TEST SYNC) ENDED" << "\n";
	}

	std::cout << "-------------------------------------------------" << "\n";

	//Тест 3.
	{
		std::cout << "TEST 3 (POP TEST) STARTED" << "\n";

		threadsafe_queue<int> ourQueue{};
		ourQueue.push(200);
		ourQueue.push(300);

		std::thread thr(tryPopTest, std::ref(ourQueue));
		thr.detach();
		
		std::cout << "Try_pop complete - " << *ourQueue.try_pop() << "\n";

		ourQueue.printAndCrearData(); //Удалится 200, а после окончания теста 300. 

		std::cout << "TEST 3 (POP TEST) ENDED" << "\n";
	}

	std::cout << "-------------------------------------------------" << "\n";
	std::this_thread::sleep_for(std::chrono::microseconds(200)); //Ждем пока синхронизируются потоки с прошлого теста.

	//Тест 4.
	{
		std::cout << "TEST 4 (POP WAIT TEST) STARTED" << "\n";

		threadsafe_queue<int> ourQueue{};
		ourQueue.push(200);
		ourQueue.push(300);

		std::thread thr(tryPopTest, std::ref(ourQueue));
		thr.detach();
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); //Подождали открепленный поток.

		std::cout << "Wait_pop complete - " << *ourQueue.wait_and_pop() << "\n";

		ourQueue.printAndCrearData(); //Удалится 200, а после окончания теста 300. Т.к. один поток заблокировал другой.

		std::cout << "TEST 4 (POP WAIT TEST) ENDED" << "\n";
	}

	std::cout << "-------------------------------------------------" << "\n";
	std::this_thread::sleep_for(std::chrono::microseconds(200)); //Ждем пока синхронизируются потоки с прошлого теста.


	system("pause");
	
}