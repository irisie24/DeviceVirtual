#ifndef ACTUATION_
#define ACTUATION_

#include <list>
#include <string>
#include <filesystem>
#include <chrono>
#include <map>
#include "Configuration.hpp"

typedef enum RuleConditionType
{
	ConditionGreaterThan,
	ConditionLessThan,
	ConditionEqualTo,
	ConditionGreaterOrEqual,
	ConditionLessOrEqual,
	ConditionNotEqualTo,
	ConditionClock,
	ConditionUnknown
}RuleConditionType;

typedef enum ActuatorActionType
{
	ActionGpio,
	ActionPwm,
	ActionUnknown
}ActuatorActionType;

typedef struct Rule
{
	unsigned long id;
	RuleConditionType condition;
	long sensorValue;
	ActuatorActionType actionType;
	unsigned long actionIo;
	long actionValue;
	bool ruleChained;
	unsigned long nextRuleTrue;
	unsigned long nextRuleFalse;
}Rule;

typedef struct ActuationDecision
{
	unsigned long ruleId;
	bool conditionMatched;
	ActuatorActionType actionType;
	unsigned long actionIo;
	long actionValue;
}ActuationDecision;

class ActuationCallback
{
public:
	virtual void onActionTriggered(const ActuationDecision& decision) = 0;
	virtual void onRuleEvaluationComplete(long sensorValue) = 0;
};

class Actuation
{
public:
	Actuation();
	~Actuation();

	void setDeviceId(const std::string& deviceId);
	std::string getDeviceId() const;

	bool loadRuleDefinition(Configuration& ruleConfiguration);
	bool evaluate(long sensorValue, ActuationCallback* callback);
	void clearRules();
	std::list<Rule> getRules();

private:
	RuleConditionType parseConditionType(const std::string& value);
	ActuatorActionType parseActionType(const std::string& value);
	bool evaluateCondition(long sensorValue, RuleConditionType condition, long ruleValue);
	bool evaluateClockCondition(const Rule& rule);
	unsigned long parseUnsignedLong(const std::string& value);
	long parseLong(const std::string& value);
	Rule* findRuleById(unsigned long ruleId);

	std::list<Rule> _rules;
	bool _repeatRuleSequence;
	unsigned long _entryRuleId;
	std::string _deviceId;
	std::map<unsigned long, std::chrono::steady_clock::time_point> _lastClockTrigger;
};

#endif
