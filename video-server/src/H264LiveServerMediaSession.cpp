#include "H264LiveServerMediaSession.h"

H264LiveServerMediaSession::Setup::Setup(
		UsageEnvironment& env, bool reuseFirstSource, const std::string& cameraSource, std::size_t cameraIndex,
		const cv::Size& outSize)
	: env(env), reuseFirstSource(reuseFirstSource), cameraSource(cameraSource), cameraIndex(cameraIndex),
	  outSize(outSize) {
}

H264LiveServerMediaSession* H264LiveServerMediaSession::createNew(const Setup& setup) {
	return new H264LiveServerMediaSession(setup);
}

H264LiveServerMediaSession::H264LiveServerMediaSession(const Setup& setup) :
		OnDemandServerMediaSubsession(setup.env, setup.reuseFirstSource),
		setup(setup),
		fAuxSDPLine(NULL), fDoneFlag(0), fDummySink(NULL), source(NULL) {
}

H264LiveServerMediaSession::~H264LiveServerMediaSession(void) {
	delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void* clientData) {
	H264LiveServerMediaSession *session = (H264LiveServerMediaSession*) clientData;
	session->afterPlayingDummy1();
}

void H264LiveServerMediaSession::afterPlayingDummy1() {
	envir().taskScheduler().unscheduleDelayedTask(nextTask());
	setDoneFlag();
}

void H264LiveServerMediaSession::abort() {
	if (source) {
		source->abort();
	}
}

static void checkForAuxSDPLine(void* clientData) {
	H264LiveServerMediaSession* session = (H264LiveServerMediaSession*) clientData;
	session->checkForAuxSDPLine1();
}

void H264LiveServerMediaSession::checkForAuxSDPLine1() {
	char const* dasl;
	if (fAuxSDPLine != NULL) {
		setDoneFlag();
	} else if (fDummySink != NULL && (dasl = fDummySink->auxSDPLine()) != NULL) {
		fAuxSDPLine = strDup(dasl);
		fDummySink = NULL;
		setDoneFlag();
	} else {
		int uSecsDelay = 100000;
		nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsDelay, (TaskFunc*) checkForAuxSDPLine, this);
	}
}

char const* H264LiveServerMediaSession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
	if (fAuxSDPLine != NULL)
		return fAuxSDPLine;
	if (fDummySink == NULL) {
		fDummySink = rtpSink;
		fDummySink->startPlaying(*inputSource, afterPlayingDummy, this);
		checkForAuxSDPLine(this);
	}

	envir().taskScheduler().doEventLoop(&fDoneFlag);
	return fAuxSDPLine;
}

FramedSource* H264LiveServerMediaSession::createNewStreamSource(unsigned clientSessionID, unsigned& estBitRate) {
	// Based on encoder configuration i kept it 90000
	estBitRate = 90000;

	source = LiveSourceWithx264::createNew(LiveSourceWithx264::Setup(
			envir(),
			setup.cameraSource,
			setup.cameraIndex,
			setup.outSize
	));
	// are you trying to keep the reference of the source somewhere? you shouldn't.
	// Live555 will create and delete this class object many times. if you store it somewhere
	// you will get memory access violation. instead you should configure you source to always read from your data source
	return H264VideoStreamDiscreteFramer::createNew(envir(), source);
}

RTPSink* H264LiveServerMediaSession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) {
	return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
