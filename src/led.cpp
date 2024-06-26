/* 
 *  led.cpp is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
 *  
 *  Copyright 2022 Alexander Reinert
 *  
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *  
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *  
 */

#include "led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static uint8_t _blinkState = 0;
static LED *_leds[MAX_LED_COUNT] = {0};
static TaskHandle_t _switchTaskHandle = NULL;
static int _highDuty;

void ledSwitcherTask(void *parameter)
{
    for (;;)
    {
        _blinkState = (_blinkState + 1) % 24;

        for (uint8_t i = 0; i < MAX_LED_COUNT; i++)
        {
            if (_leds[i] == 0)
            {
                break;
            }
            _leds[i]->updatePinState();
        }
        vTaskDelay(125 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void LED::start(Settings *settings)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_11_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };

    _highDuty = settings->getLEDBrightness() * (1 << ledc_timer.duty_resolution) / 100;

    ledc_timer_config(&ledc_timer);

    if (!_switchTaskHandle)
    {
        xTaskCreate(ledSwitcherTask, "LED_Switcher", 4096, NULL, 10, &_switchTaskHandle);
    }
}

void LED::stop()
{
    if (_switchTaskHandle)
    {
        vTaskDelete(_switchTaskHandle);
        _switchTaskHandle = NULL;
    }
}

LED::LED(gpio_num_t pin)
{
    _channel_conf.gpio_num = pin;

    for (uint8_t i = 0; i < MAX_LED_COUNT; i++)
    {
        if (_leds[i] == 0)
        {
            _channel_conf.channel = (ledc_channel_t)i;
            break;
        }
    }

    ledc_channel_config(&_channel_conf);

    _leds[_channel_conf.channel] = this;
}

void LED::setState(led_state_t state)
{
    _state = state;
    updatePinState();
}

void LED::_setPinState(bool enabled) {
    ledc_set_duty(_channel_conf.speed_mode, _channel_conf.channel, enabled ? _highDuty : 0);
    ledc_update_duty(_channel_conf.speed_mode, _channel_conf.channel);
}

void LED::updatePinState()
{
    switch (_state)
    {
    case LED_STATE_OFF:
        _setPinState(false);
        break;
    case LED_STATE_ON:
        _setPinState(true);
        break;
    case LED_STATE_BLINK:
        _setPinState((_blinkState % 8) < 4);
        break;
    case LED_STATE_BLINK_INV:
        _setPinState((_blinkState % 8) >= 4);
        break;
    case LED_STATE_BLINK_FAST:
        _setPinState((_blinkState % 2) == 0);
        break;
    case LED_STATE_BLINK_SLOW:
        _setPinState(_blinkState < 12);
        break;
    }
}