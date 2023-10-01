#pragma once

#include <dsqmlapplicationengine.h>
#include <qjsondocument.h>


namespace dsqt {

/**
 * \class tcs::BridgeService
 */
class BridgeService {
  public:
    explicit BridgeService(DSQmlApplicationEngine* engine);
	~BridgeService();

	void start();
	void stop();

  private:
	class Loop final : public Poco::Runnable {
	  public:
		explicit Loop(ds::ui::SpriteEngine& engine);

		void run() override;

		// Signal the background thread that new content is available.
		void refresh();
		// Signal the background thread that it should abort.
		void abort();

	  private:
		///
		bool eventIsNow(ds::model::ContentModelRef& event, Poco::DateTime& ldt) const;
		///
		void loadContent();
		///
		void updatePlatformEvents() const;

		ci::app::AppBase* mApp =
			nullptr; // Pointer to main application, allowing us to execute code on the main thread.

		ds::ui::SpriteEngine&	   mEngine;			// Reference to the sprite engine.
		ds::model::ContentModelRef mContent;		//
		ds::model::ContentModelRef mPlatforms;		//
		ds::model::ContentModelRef mEvents;			//
		ds::model::ContentModelRef mRecords;		//
		QMutex				   mMutex;			// Controls access to mAbort and mRefresh.
		bool					   mAbort;			// If true, will abort the background thread.
		bool					   mRefresh;		// If true, new content is available.
		const long				   mRefreshRateMs;	// in milliseconds
		int						   mResourceId = 1; //
	};

	ds::ui::SpriteEngine&  mEngine;		 //
	ds::DelayedNodeWatcher mNodeWatcher; //
	Poco::Thread		   mThread;		 //
	Loop				   mLoop;		 //
};

} // namespace downstream
