#include "Controller/ControllerType/JointController/RJointController.h"
#include "ROSTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogRJointController, Log, All);

URJointController::URJointController()
{
  Mode = UJointControllerMode::Dynamic;
  bDisableCollision = false;
  bControllAllJoints = false;
  EnableDrive.PositionStrength = 1E5;
  EnableDrive.VelocityStrength = 1E4;
  EnableDrive.MaxForce = 1E10;
}

void URJointController::SetControllerParameters(URControllerParameter *&ControllerParameters)
{
  URJointControllerParameter *JointControllerParameters = Cast<URJointControllerParameter>(ControllerParameters);
  if (JointControllerParameters)
  {
    Mode = JointControllerParameters->Mode;
    bDisableCollision = JointControllerParameters->bDisableCollision;
    bControllAllJoints = JointControllerParameters->bControllAllJoints;
    EnableDrive = JointControllerParameters->EnableDrive;
  }
}

void URJointController::Init()
{
  Super::Init();

  bPublishResult = false;
  if (GetOwner())
  {
    SetMode();

    for (const TPair<FString, FJointState> &DesiredJointState : DesiredJointStates)
      {
        const FString JointName = DesiredJointState.Key;
        if (URJoint *Joint = GetOwner()->GetJoint(JointName))
          {
            Joint->SetDrive(EnableDrive);
            Joint->SetMotorJointState(DesiredJointStates.FindRef(JointName));
          }
        else
          {
            UE_LOG(LogRJointController, Error, TEXT("%s of %s not found"), *JointName, *GetOwner()->GetName());
          }
      }


    if (bControllAllJoints)
      {
        TArray<FString> JointNames;
        for (URJoint *&Joint : GetOwner()->GetJoints())
          {
            JointNames.Add(Joint->GetName());
          }
        SetJointNames(JointNames);
      }
  }
  else
  {
    UE_LOG(LogRJointController, Error, TEXT("%s is not attached to ARModel"), *GetName())
  }
}

void URJointController::SetMode()
{
  if (GetOwner())
  {
    bool bEnablePhysics = true;
    switch (Mode)
    {
    case UJointControllerMode::Kinematic:
      EnableDrive.bPositionDrive = false;
      EnableDrive.bVelocityDrive = false;
      bEnablePhysics = false;
      break;

    case UJointControllerMode::Dynamic:
      EnableDrive.bPositionDrive = true;
      EnableDrive.bVelocityDrive = true;
      for (URLink *&Link : GetOwner()->GetLinks())
      {
        if (bDisableCollision)
        {
          Link->DisableCollision();
        }
        Link->GetCollision()->SetEnableGravity(false);
      }
      break;
    }

    for (URJoint *&Joint : GetOwner()->GetJoints())
    {
      Joint->Child->GetCollision()->SetSimulatePhysics(bEnablePhysics);
    }
  }
}

void URJointController::SetJointNames(const TArray<FString> &JointNames, const FEnableDrive &InEnableDrive)
{
  for (const FString &JointName : JointNames)
  {
    if (!DesiredJointStates.Contains(JointName) && GetOwner()->GetJoint(JointName))
    {
      DesiredJointStates.Add(JointName, FJointState());
      GetOwner()->GetJoint(JointName)->SetDrive(InEnableDrive);
    }
  }
}

void URJointController::SetJointNames(const TArray<FString> &JointNames)
{
  for (const FString &JointName : JointNames)
  {
    if (!DesiredJointStates.Contains(JointName) && GetOwner()->GetJoint(JointName))
    {
      DesiredJointStates.Add(JointName, FJointState());
      GetOwner()->GetJoint(JointName)->SetDrive(EnableDrive);
    }
  }
}

void URJointController::Tick(const float &InDeltaTime)
{
  if (GetOwner())
  {
    // for (auto &Joint : GetOwner()->Joints)
    // {
    //   Joint.Value->UpdateEncoder();
    // }

    MoveJoints(InDeltaTime);
  }
  else
  {
    UE_LOG(LogRJointController, Error, TEXT("Owner of %s not found"), *GetName())
  }
}

void URJointController::MoveJoints(const float &InDeltaTime)
{
  switch (Mode)
  {
  case UJointControllerMode::Kinematic:
    MoveJointsKinematic();
    break;

  case UJointControllerMode::Dynamic:
    MoveJointsDynamic(InDeltaTime);
    break;
  }
}

void URJointController::MoveJointsDynamic(const float &InDeltaTime)
{
  for (const TPair<FString, FJointState> &DesiredJointState : DesiredJointStates)
  {
    const FString JointName = DesiredJointState.Key;
    if (URJoint *Joint = GetOwner()->GetJoint(JointName))
    {
      FJointState TempJointState = DesiredJointStates.FindRef(JointName);
      TempJointState.JointVelocity = CalculateJointVelocity(InDeltaTime, JointName);
      Joint->SetMotorJointState(TempJointState);
      // Joint->SetMotorJointState(DesiredJointStates.FindRef(JointName));
    }
    else
    {
      UE_LOG(LogRJointController, Error, TEXT("%s of %s not found"), *JointName, *GetOwner()->GetName())
    }
  }
}

void URJointController::MoveJointsKinematic()
{
  for (const TPair<FString, FJointState> &DesiredJointState : DesiredJointStates)
  {
    const FString JointName = DesiredJointState.Key;
    if (URJoint *Joint = GetOwner()->GetJoint(JointName))
    {
      Joint->SetJointPosition(DesiredJointStates.FindRef(JointName).JointPosition, nullptr);
    }
    else
    {
      UE_LOG(LogRJointController, Error, TEXT("%s of %s not found"), *JointName, *GetOwner()->GetName())
    }
  }
}

float URJointController::CalculateJointVelocity(float InDeltaTime, FString InJointName)
{
  float DesiredPos = DesiredJointStates[InJointName].JointPosition;
  URJoint *Joint = GetOwner()->Joints[InJointName];
  float CurrentJointPos = Joint->GetEncoderValue();
  float Diff = DesiredPos - CurrentJointPos;

  // float Vel = Diff / InDeltaTime;
  float Vel = Diff ;

  // UE_LOG(LogRJointController, Error, TEXT("%s: Diff %f Vel %f"), *InJointName, Diff, Vel);
  return Vel;

}
