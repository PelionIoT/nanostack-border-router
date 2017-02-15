// List of targets to compile
def targets = [
  "K64F"
  ]

// Map toolchains to compilers
def toolchains = [
  ARM: "armcc",
  GCC_ARM: "arm-none-eabi-gcc",
  IAR: "iar_arm"
  ]

// Configurations
def configurations = [
  LOWPAN: "6lowpan_K64F_Atmel_RF_config.json",
  THREAD: "Thread_K64F_Atmel_RF_config.json",
  THREAD_SLIP: "Thread_SLIP_K64F_Atmel_RF_config.json"
  ]

def stepsForParallel = [:]

// Jenkins pipeline does not support map.each, we need to use oldschool for loop
for (int i = 0; i < targets.size(); i++) {
  for(int j = 0; j < toolchains.size(); j++) {
    for(int k = 0; k < configurations.size(); k++) {
      def target = targets.get(i)
      def toolchain = toolchains.keySet().asList().get(j)
      def compilerLabel = toolchains.get(toolchain)
      def configurationLabel = configurations.keySet().asList().get(k)
      def configurationFile = configurations.get(configurationLabel)
      def stepName = "${target} ${configurationLabel} ${toolchain}"
      stepsForParallel[stepName] = buildStep(target, compilerLabel, configurationFile, configurationLabel, toolchain)
    }
  }
}

timestamps {
  parallel stepsForParallel
}

def buildStep(target, compilerLabel, configurationFile, configurationLabel, toolchain) {
  return {
    stage ("${target}_${compilerLabel}_${configurationLabel}") {
      node ("${compilerLabel}") {
        deleteDir()
        dir("k64f-border-router-private") {
          checkout scm
          execute("mbed deploy --protocol ssh")
          execute("mbed compile --build out/${target}_${configurationLabel}_${compilerLabel}/ -m ${target} -t ${toolchain} --app-config ./configs/${configurationFile} -c")
        }
        archive '**/nanostack-border-router.bin'
      }
    }
  }
}
