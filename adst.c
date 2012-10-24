// get time & date from GPS
// adjust for time zone
// if (secs == 0) calc DST offset
// adjust hour, minute for new offset
// set RTC time

// following fragment from "fix time" sub
	if (time_s == 0) {  // done once a minute - overkill but it will update for DST when clock is booted or time is adjusted
    if (dst_mode == DST_AUTO)  //Check daylight saving time.
			setDSToffset(dst_rules);  // set DST offset based on DST rules
		else if(dst_mode == DST_ON) 
			dst_offset = 1;  // fixed offset
		else
			dst_offset = 0;  // no offset
		if ((time_m == 0) && (time_h == 0))  // Midnight?
			dst_update = DST_YES;  // Reset DST Update flag at midnight
	}
// end fragment

long yearSeconds(uint8_t yr, uint8_t mo, uint8_t da, uint8_t h, uint8_t m, uint8_t s)
{
  long dn;
  dn = monthDays[(mo-1)]+da;  // # days so far if not leap year
  if ((yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0)  // if leap year
    dn ++;  // add 1 day
  dn = dn * 86400 + h*3600 + m*60 + s;
  return dn;
} 

long DSTseconds(uint8_t month, uint8_t doftw, uint8_t n, uint8_t hour)
{
	uint8_t dom = monthDays[month-1];
	if ( (date_y%4 == 0) && (month == 2) )
		dom ++;  // february has 29 days this year
	uint8_t dow = dotw(date_y, month, 1);  // DOW for 1st day of month for DST event
	uint8_t day = doftw - dow;  // number of days until 1st dotw in given month
//	if (day<1)  day += 7;  // make sure it's positive - doesn't work with uint!
  if (doftw >= dow)
    day = doftw - dow;
  else
    day = doftw + 7 - dow;
	day = 1 + day + (n-1)*7;  // date of dotw for this year
	while (day > dom)  // handles "last DOW" case
		day -= 7;
  return yearSeconds(date_y,month,day,hour,0,0);  // seconds til DST event this year
}

// DST Rules: Start(month, dotw, n, hour), End(month, dotw, n, hour), Offset
// DOTW is Day of the Week.  1=Sunday, 7=Saturday
// N is which occurrence of DOTW
// Current US Rules: March, Sunday, 2nd, 2am, November, Sunday, 1st, 2 am, 1 hour
// 		3,1,2,2,  11,1,1,2,  1
void setDSToffset(uint8_t rules[9])
{
	uint8_t month1 = rules[0];
	uint8_t dotw1 = rules[1];
	uint8_t n1 = rules[2];  // nth dotw
	uint8_t hour1 = rules[3];
	uint8_t month2 = rules[4];
	uint8_t dotw2 = rules[5];
	uint8_t n2 = rules[6];
	uint8_t hour2 = rules[7];
	uint8_t offset = rules[8];
	// if current time & date is at or past the first DST rule and before the second, set dst_offset, otherwise reset dst_offset
  long seconds1 = DSTseconds(month1,dotw1, n1, hour1);  // seconds til start of DST this year
  long seconds2 = DSTseconds(month2,dotw2, n2, hour2);  // seconds til end of DST this year
  long seconds_now = yearSeconds(date_y, date_m, date_d, time_h, time_m, time_s);
	if ((seconds_now >= seconds1) && (seconds_now < seconds2))
	{  // spring ahead
		if ((dst_offset == 0) && (dst_update == DST_YES)) {
			dst_update = DST_NO;  // Only do this once per day
			dst_offset = offset;
			time_h += offset;  // if this is the first time, bump the hour
//??			eeprom_write_byte((uint8_t *)EE_DSTOFFSET, offset);  // remember setting for power up
//??			eeprom_write_byte((uint8_t *)EE_HOUR, time_h);    
		}
	}
	else
	{  // fall back
		if ((dst_offset >0) && (dst_update == DST_YES)) {
			dst_update = DST_NO;  // Only do this once per day
			dst_offset = 0;
			time_h -= offset;  // if this is the first time, bump the hour
//??			eeprom_write_byte((uint8_t *)EE_DSTOFFSET, offset);  // remember setting for power up
//??			eeprom_write_byte((uint8_t *)EE_HOUR, time_h);    
		}
	}
}
