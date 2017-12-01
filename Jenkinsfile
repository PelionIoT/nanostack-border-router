properties ([[$class: 'ParametersDefinitionProperty', parameterDefinitions: [
  [$class: 'StringParameterDefinition', name: 'mbed_os_revision', defaultValue: '', description: 'Revision of mbed-os to build. To access mbed-os PR use format "pull/PR number/head". To use ARMC6/IAR8.2 use label "newtoolchain']
  ]]])

if (params.mbed_os_revision == '') {
  echo 'Use mbed OS revision from mbed-os.lib'
} else if (params.mbed_os_revision == 'newtoolchain') {
  echo 'Use ARMC6 and IAR8.2 feature branch'
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
  // ARM: "armcc", ARM is replaced by ARMC6
  ARMC6: "armc6",
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
              } else if (params.mbed_os_revision == 'newtoolchain') {
                // magic to get feature branches and new nanostack binaries
                if (toolchain == "IAR") {
                  echo 'Use IAR8.2 feature branch'
                  // Temp for IAR8.2, get feature branch and new nanostack binaries
                  execute ("git checkout feature-iar8")
                  // Fetch IAR8.2 compiled binaries
                  execute("git fetch origin pull/5402/head:_IAR82_BINARY_PR_")
                  execute("git checkout _IAR82_BINARY_PR_")
                  execute("git pull origin feature-iar8 ")
                  execute("git log -n 5")
                } else if (toolchain == "ARMC6") {
                  echo 'Use ARMC6 feature branch'
                  // Temp for ARMC6, get feature branch and pick ARMc6 binaries
                  execute ("git checkout feature-armc6")
                  // Fetch ARMC6 compiled binaries
                  execute("git fetch origin pull/5401/head:_ARMC6_BINARY_PR_")
                  execute("git checkout _ARMC6_BINARY_PR_")
                  execute("git pull origin feature-armc6 ")
                  execute("git log -n 5")
                }
              } else {
                  execute ("git checkout ${params.mbed_os_revision}")
              }
            }
          }
          // Ensure ARM builds are always in ARM folder
          def build_toolchain_folder = toolchain
          if (build_toolchain_folder == "ARMC6") {
            //Replace ARMC6 with ARM (no need to modify test suites)
            build_toolchain_folder = "ARM"
          }

          execute("mbed compile --build out/${configurationLabel}/${target}/${build_toolchain_folder}/ -m ${target} -t ${toolchain} --app-config ./configs/${configurationFile}")
        }
        archive '**/nanostack-border-router.bin'
      }
    }
  }
}
