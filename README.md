# Petkit_Epaper_Dashboard
![image of epaper dashboard diplay](resources/dashboard.jpg)
Catto Dashbord using Seeedstudio reterminal epaper displays E1001 or E1002

Compatible with petkit automatic litterboxes, it collects data directly from petkit servers to help track pet weight and liitterbox usage patterns. 

It should support up to four cats, and requires no setup besides using the captive portal to enter your wifi details and petkit login details. 

It stores 365 days of past usage data to its micro SD card, and has selectable plot date ranges. It contacts petkit servers to request the most recent litterbox usage data every 2 hours, and spends the vast majority of time in deep sleep to conserve battery. 

It is able to determine your local timezone automatically, and synchronize itself and the built in RTC using NTP servers. Be sure to add a CR1225 battery to the holder inside, it does not come with one installed.
