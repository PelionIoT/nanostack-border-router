properties ([[$class: 'ParametersDefinitionProperty', parameterDefinitions: [
  [$class: 'StringParameterDefinition', name: 'mbed_os_revision', defaultValue: '', description: 'Revision of mbed-os to build. By default mbed-os.lib is used. To access mbed-os PR use format "pull/PR number/head"']
  ]]])

if (params.mbed_os_revision == '') {
  echo 'Use mbed OS revision from mbed-os.lib'
} else {
  echo "Use mbed OS revision ${params.mbed_os_revision}"
  if (params.mbed_os_revision.matches('pull/\\d+/head')) {
    echo "Revision is a Pull Request"
  }
}

// List of targets to compile
def targets = [
  "K64F",
  "K66F",
  "NUCLEO_F429ZI"
  ]

// Map toolchains to CI labels
def toolchains = [
  ARM: "armcc",
//  ARMC6: "armc6",
  GCC_ARM: "arm-none-eabi-gcc",
  IAR: "iar_arm"
  ]

// Configurations
def configurations = [
  LOWPAN: "6lowpan_Atmel_RF.json",
  THREAD: "Thread_Atmel_RF.json",
  THREAD_SLIP: "Thread_SLIP_Atmel_RF.json"
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
      // SLIP configuration exist only for K64F based Raspberry HAT
      if (configurationLabel == "THREAD_SLIP" && target != "K64F") {
        continue;
      }
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
        dir("nanostack-border-router") {
          checkout scm
          execute("mbed deploy --protocol ssh")
          // Update mbed-os revision if requested
          if (params.mbed_os_revision != '') {
            dir ("mbed-os") {
              if (params.mbed_os_revision.matches('pull/\\d+/head')) {
                execute("git fetch origin ${params.mbed_os_revision}:_PR_")
                execute("git checkout _PR_")
              } else {
                execute ("git checkout ${params.mbed_os_revision}")
              }
            }
          }
          execute("mbed compile --build out/${configurationLabel}/${target}/${toolchain}/ -m ${target} -t ${toolchain} --app-config ./configs/${configurationFile}")
        }
        archive '**/nanostack-border-router.bin'
      }
    }
  }
}
