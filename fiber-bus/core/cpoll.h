#pragma once
#include <cinttypes>
#include <vector>
#include <sys/epoll.h>

namespace core {
	class cepoll {
		using tevent = struct epoll_event;
		int								fdPoll{ -1 };
	public:

		enum : uint32_t { in = EPOLLIN, out = EPOLLOUT,err = EPOLLERR,pri = EPOLLPRI,hup = EPOLLHUP,edge = EPOLLET,offline = EPOLLRDHUP, oneshot = EPOLLONESHOT};
		using events = std::vector<cepoll::tevent>;

		cepoll();
		~cepoll();

		void shutdown();

		/**
		 * Элемент events является набором битов, созданном с помощью следующих возможных типов событий:
		 * EPOLLIN  - Ассоциированный файл доступен для операций read
		 * EPOLLOUT - Ассоциированный файл доступен для операций write
		 * EPOLLPRI - Нет срочных данных, доступных для операций read
		 * EPOLLERR - Произошла ошибка на ассоциированном описателе файлов.
		 * EPOLLHUP - На ассоциированном описателе файлов произошло зависание.
		 * EPOLLET  - Устанавливает поведение Edge Triggered для ассоциированного описателя файлов. (По умолчанию Level Triggered)
		 * EPOLLRDHUP - Одна из сторон потокового сокета закрыла соединение, или выключила записывающую часть соединения. (Этот флаг особенно полезен при написании простого кода для обнаружения отключения стороны с помощью слежения Edge Triggered.)
		 * EPOLLONESHOT - Установить однократное получение для связанного файлового дескриптора.
		 *                Это означает, что после извлечения события с помощью epoll_wait(2) со связанным дескриптором приём отключается и о других событиях интерфейс epoll сообщать не будет.
		 *                Пользователь должен вызвать epoll_ctl() с операцией EPOLL_CTL_MOD для переустановки новой маски событий для файлового дескриптора.
		 *
		 *
		 * Режимы работы. (http://ru.wikipedia.org/wiki/Epoll)
		 * epoll позволяет работать в двух режимах:
		 * edge-triggered — файловый дескриптор с событием возвращается только если с момента последнего возврата произошли новые события (например, пришли новые данные)
		 * level-triggered — файловый дескриптор возвращается, если остались непрочитанные/записанные данные
		 * Если приложение прочитало из файлового дескриптора только часть доступных для чтения данных, то при следующем вызове:
		 * в edge-triggered файловый дескриптор не будет возвращён до тех пор, пока в дескрипторе не появятся новые данные;
		 * в level-triggerd файловый дескриптор будет возвращаться до тех пор, пока не прочитаны все «старые» данные (и новые, если таковые придут)
		 */
		ssize_t emplace(int fd, const uint32_t& events);
		ssize_t update(int fd, const uint32_t& events);
		ssize_t remove(int fd);
		ssize_t wait(cepoll::events& list, ssize_t timeout = -1);

	private:
		cepoll(const cepoll&) = delete;
		cepoll(const cepoll&&) = delete;
		cepoll& operator = (const cepoll&) = delete;
		cepoll& operator = (const cepoll&&) = delete;
	};
}