#include "estimator.h"

#include <string.h>

#define LAN_DISAGREE_FACTOR 3

const char *estimate_stability_confidence(int stable, int observed)
{
    if (stable <= 0)
        return "Low";

    if (observed > 0 && stable * 3 >= observed * 2)
        return "High";

    return "Medium";
}

estimate_result estimate_people(const estimate_input *input)
{
    estimate_result result;
    double factor = input->devices_per_person;

    if (factor <= 0.0)
        factor = 1.0;

    result.people = (int)((double)input->stable_bt / factor + 0.5);
    result.confidence =
        estimate_stability_confidence(input->stable_bt, input->observed_bt);
    result.lan_disagrees = 0;

    /*
     * Cross-check with Wi-Fi: if the number of other hosts on the network and
     * the Bluetooth estimate are very different, the two signals do not
     * support each other and "High" is lowered to "Medium". Zero hosts is not
     * treated as disagreement because people may not use this network.
     */
    if (input->lan_available && input->lan_hosts > 0 && result.people > 0)
    {
        int high = input->lan_hosts > result.people ? input->lan_hosts
                                                    : result.people;
        int low = input->lan_hosts > result.people ? result.people
                                                   : input->lan_hosts;

        if (high > LAN_DISAGREE_FACTOR * low)
        {
            result.lan_disagrees = 1;

            if (strcmp(result.confidence, "High") == 0)
                result.confidence = "Medium";
        }
    }

    return result;
}
