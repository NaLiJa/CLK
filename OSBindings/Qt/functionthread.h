#pragma once

#include <atomic>
#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QThread>

/*!
 * \brief The LambdaThread class
 *
 * Provides a QThread to which lambdas can be posted.
 *
 * Disclaimer: this might be a crutch that reveals a misunderstanding of the Qt
 * threading infrastructure. We'll see.
 */
class FunctionThread: public QThread {
	public:
		~FunctionThread() {
			stop();
		}

		void run() override {
			// Gymnastics here: events posted directly to the QThread will occur on the thread
			// that created the QThread. To have events occur within a QThread, they have to be
			// posted to an object created on that thread. FunctionPerformer fills that role.
			if(!performer) performer = std::make_unique<FunctionPerformer>();
			performerFlag.clear();
			exec();
		}

		void stop() {
			if(isRunning()) {
				performAsync([this] {
					this->quit();
				});
			}
			wait();
		}

		void start() {
			if(isRunning()) {
				return;
			}

			// TODO: I've assumed a race condition here with the creation of performer; if QThread
			// blocks on completion of `run` when starting then this is redundant.
			performerFlag.test_and_set();
			QThread::start();
			while(performerFlag.test_and_set());
		}

		/*!
		 * \brief Schedules a function to be performed on this thread. Control
		 * must return to the main event loop for the function to be performed;
		 * use QCoreApplication::sendPostedEvents() to ensure the function is
		 * performed before then, if required.
		 *
		 * \param function The function to perform.
		 */
		void performAsync(const std::function<void(void)> &function) {
			QApplication::instance()->postEvent(performer.get(), new FunctionEvent(function));
			QCoreApplication::sendPostedEvents();
		}

	private:
		struct FunctionEvent: public QEvent {
			FunctionEvent(const std::function<void(void)> &function) : QEvent(QEvent::Type::User), function(function) {}
			std::function<void(void)> function;
		};

		struct FunctionPerformer: public QObject {
			FunctionPerformer(): QObject() {}

			bool event(QEvent *event) override {
				if(event->type() == QEvent::Type::User) {
					const auto functionEvent = dynamic_cast<FunctionEvent *>(event);
					if(functionEvent) {
						functionEvent->function();
						return true;
					}
				}
				return QObject::event(event);
			}
		};
		std::unique_ptr<FunctionPerformer> performer;
		std::atomic_flag performerFlag;
};
