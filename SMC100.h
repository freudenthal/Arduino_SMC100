#ifndef SMC100_h	//check for multiple inclusions
#define SMC100_h

#include "Arduino.h"

#define SMC100QueueCount 8
#define SMC100ReplyBufferSize 32

class SMC100
{
	public:
		typedef void ( *FinishedListener )();
		enum class CommandType : uint8_t
		{
			None,
			Enable,
			Home,
			MoveAbs,
			MoveRel,
			MoveEstimate,
			Configure,
			Analogue,
			GPIOInput,
			Reset,
			GPIOOutput,
			LimitPositive,
			LimitNegative,
			PositionAsSet,
			PositionReal,
			KeypadEnable,
			ErrorCommands,
			ErrorHardware,
		};
		enum class CommandParameterType : uint8_t
		{
			None,
			Int,
			Float,
			ErrorCommand,
			ErrorHardware
		};
		enum class StatusType : uint8_t
		{
			Unknown,
			Error,
			NoReference,
			Homing,
			Moving,
			Ready,
			Disabled,
			Jogging,
		};
		enum class CommandGetSetType : uint8_t
		{
			None,
			Get,
			Set,
			GetSet,
			GetAlways,
		};
		enum class ModeType : uint8_t
		{
			Inactive,
			Idle,
			WaitAfterSendingCommand,
			WaitForCommandReply,
		};
		struct CommandStruct
		{
			CommandType Command;
			const char* CommandChar;
			CommandParameterType SendType;
			CommandGetSetType GetSetType;
		};
		struct CommandQueueEntry
		{
			const CommandStruct* Command;
			CommandGetSetType GetOrSet;
			float Parameter;
		};
		struct StatusCharSet
		{
			const char* Code;
			StatusType Type;
		};
		SMC100(HardwareSerial* serial, uint8_t address);
		void Check();
		void Begin();
		bool IsHomed();
		bool IsReady();
		bool IsMoving();
		bool IsEnabled();
		void Enable(bool Setting);
		bool IsBusy();
		void Home();
		void MoveAbsolute(float Target);
		void SetGPIOOutput(uint8_t Pin, bool Output);
		void SetGPIOOutputAll(uint8_t Code);
		void SendGetGPIOInput();
		bool GetGPIOInput(uint8_t Pin);
		void SetAllCompleteCallback(FinishedListener Callback);
		void SetHomeCompleteCallback(FinishedListener Callback);
		void SetMoveCompleteCallback(FinishedListener Callback);
		float GetPosition();
	private:
		void CheckCommandQueue();
		void CheckForCommandReply();
		void CheckWaitAfterSending();
		void ClearCommandQueue();
		bool SendCurrentCommand();
		bool CommandQueueFull();
		bool CommandQueueEmpty();
		uint8_t CommandQueueCount();
		void CommandQueueAdvance();
		void CommandQueueRetreat();
		void CommandCurrentPut(CommandType Type, float Parameter, CommandGetSetType GetOrSet);
		void CommandQueuePut(CommandType Type, float Parameter, CommandGetSetType GetOrSet);
		void CommandQueuePut(const CommandStruct* CommandPointer, float Parameter, CommandGetSetType GetOrSet);
		bool CommandQueuePullToCurrentCommand();
		void SendGetLimitNegative();
		void SendGetLimitPositive();
		void SendErrorCommandRequest();
		void SendErrorHardwareRequest();
		void SendPositionRequest();
		StatusType ConvertStatus(char* StatusChar);
		void ParseReply();
		static const CommandStruct CommandLibrary[];
		static const StatusCharSet StatusLibrary[];
		static const uint32_t CommandReplyTimeMax;
		static const uint32_t WipeInputEvery;
		static const char CarriageReturnCharacter;
		static const char NewLineCharacter;
		static const char GetCharacter;
		static const uint32_t WaitAfterSendingTimeMax;
		static const char NoErrorCharacter;
		ModeType Mode;
		StatusType Status;
		bool Busy;
		bool HasBeenHomed;
		float Position;
		HardwareSerial* SerialPort;
		FinishedListener AllCompleteCallback;
		FinishedListener MoveCompleteCallback;
		bool NeedToFireMoveComplete;
		FinishedListener HomeCompleteCallback;
		bool NeedToFireHomeComplete;
		const CommandStruct* CurrentCommand;
		CommandGetSetType CurrentCommandGetOrSet;
		float CurrentCommandParameter;
		uint8_t GPIOInput;
		uint8_t GPIOOutput;
		float AnalogueReading;
		float PositionLimitNegative;
		float PositionLimitPositive;
		uint8_t Address;
		uint32_t LastWipeTime;
		uint32_t TransmitTime;
		uint8_t ReplyBufferIndex;
		char ReplyBuffer[SMC100ReplyBufferSize];
		CommandQueueEntry CommandQueue[SMC100QueueCount];
		uint8_t CommandQueueHead;
		uint8_t CommandQueueTail;
		bool CommandQueueFullFlag;
};
#endif
