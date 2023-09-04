using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

namespace Oasis.GameEvents
{
    public class BaseGameEventListener : MonoBehaviour, IPrioritized
    {
        public PriorityLevel priorityLevel = PriorityLevel.Low;
        
        public PriorityLevel Priority => priorityLevel;
    }
}