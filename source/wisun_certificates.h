/*
 * Copyright (c) 2019, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WISUN_TEST_CERTIFICATES_H_
#define WISUN_TEST_CERTIFICATES_H_

const uint8_t WISUN_ROOT_CERTIFICATE[] = {
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIBITCByaADAgECAgkAlbRr8sff1TAwCgYIKoZIzj0EAwIwDTELMAkGA1UEAwwC\r\n"
    "Q0EwIBcNMTkwMTEwMTIzOTQwWhgPMjA1NDAxMDExMjM5NDBaMA0xCzAJBgNVBAMM\r\n"
    "AkNBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEtxIrEZAp/o5tRajuwX89N/R7\r\n"
    "aWnqBb0qqfWMz8eV4qIGDZ6nTVU8WnDbGfQmMiVJ7jBDO0t0u8hdJqD+BZSRTKMQ\r\n"
    "MA4wDAYDVR0TBAUwAwEB/zAKBggqhkjOPQQDAgNHADBEAiAq2dDK4qq3tmJ3oG+T\r\n"
    "+Sn3tTkJzh98EmbD+qM3H1A8bAIgbaeMCHBMVu+gsUvsr3GE0oPFivabSbG1ACPY\r\n"
    "091AY8s=\r\n"
    "-----END CERTIFICATE-----"
};

const uint8_t WISUN_SERVER_CERTIFICATE[] = {
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIBbjCCARUCCQDauDDaJgvpkTAKBggqhkjOPQQDAjANMQswCQYDVQQDDAJDQTAe\r\n"
    "Fw0xOTAxMTAxMjU1MThaFw0yMTEwMzAxMjU1MThaMHIxCzAJBgNVBAYTAkZJMQ0w\r\n"
    "CwYDVQQIDARPVUxVMQ0wCwYDVQQHDARPVUxVMQ0wCwYDVQQKDARURVNUMQ0wCwYD\r\n"
    "VQQLDARURVNUMQ0wCwYDVQQDDARURVNUMRgwFgYJKoZIhvcNAQkBFgl0ZXN0QHRl\r\n"
    "c3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATql/+fLHybIT1ffwtvWCRQo17+\r\n"
    "NxYNHsqcLC7EwTaEFZ16Jq5ZfJONfdgi9JAhaJqPR5C39mHWRPrrb7Yz+WaxMAoG\r\n"
    "CCqGSM49BAMCA0cAMEQCIBEVE5m35FH4/x12+4CGED5DTjq+MlG4tA9qzbRV1fLR\r\n"
    "AiAVyDNhfHjqtSUHhq6n4eVFrkEZIKL15ghq/XrsquYpQA==\r\n"
    "-----END CERTIFICATE-----"
};

const uint8_t WISUN_SERVER_KEY[] = {
    "-----BEGIN EC PRIVATE KEY-----\r\n"
    "MHcCAQEEILFyZOLupuFXvz8geCxYzno3yJsmvs5MOH5IAM2+BUNToAoGCCqGSM49\r\n"
    "AwEHoUQDQgAE6pf/nyx8myE9X38Lb1gkUKNe/jcWDR7KnCwuxME2hBWdeiauWXyT\r\n"
    "jX3YIvSQIWiaj0eQt/Zh1kT662+2M/lmsQ==\r\n"
    "-----END EC PRIVATE KEY-----"
};

const uint8_t WISUN_CLIENT_CERTIFICATE[] = {
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIBbzCCARUCCQDauDDaJgvpkDAKBggqhkjOPQQDAjANMQswCQYDVQQDDAJDQTAe\r\n"
    "Fw0xOTAxMTAxMjU0NThaFw0yMTEwMzAxMjU0NThaMHIxCzAJBgNVBAYTAkZJMQ0w\r\n"
    "CwYDVQQIDARPVUxVMQ0wCwYDVQQHDARPVUxVMQ0wCwYDVQQKDARURVNUMQ0wCwYD\r\n"
    "VQQLDARURVNUMQ0wCwYDVQQDDARURVNUMRgwFgYJKoZIhvcNAQkBFgl0ZXN0QHRl\r\n"
    "c3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASiDwGGvooYkL98jjqiuIjNiY42\r\n"
    "0Yp8EnZcT5QBfm2AHBN8Cv6ZLqatnOYW2qcBobTGNWYhjEiQSXFZWCbtTOrtMAoG\r\n"
    "CCqGSM49BAMCA0gAMEUCIQCIa6wOCi56WXsMTYszQtS1HdRGWZbW9eJmtNAkrtu+\r\n"
    "4QIgNXPvNTU/0QTEkssBp1olJI93sohauvLpcXjk89e9AOA=\r\n"
    "-----END CERTIFICATE-----"
};

const uint8_t WISUN_CLIENT_KEY[] = {
    "-----BEGIN EC PRIVATE KEY-----\r\n"
    "MHcCAQEEIHKcVfg7aFwGqGnSph+XWaXoEcqrmR87s938l3B1NHLeoAoGCCqGSM49\r\n"
    "AwEHoUQDQgAEog8Bhr6KGJC/fI46oriIzYmONtGKfBJ2XE+UAX5tgBwTfAr+mS6m\r\n"
    "rZzmFtqnAaG0xjVmIYxIkElxWVgm7Uzq7Q==\r\n"
    "-----END EC PRIVATE KEY-----"
};

#endif /* WISUN_TEST_CERTIFICATES_H_ */
