#include "lib_utility_rate_equations.h"

#include <sstream>

void rate_data::init_energy_rates(bool gen_only) {
	// calculate the monthly net energy per tier and period based on units
	int c = 0;
	for (int m = 0; m < (int)m_month.size(); m++)
	{
		// check for kWh/kW
		int start_tier = 0;
		int end_tier = (int)m_month[m].ec_tou_ub.ncols() - 1;
		int num_periods = (int)m_month[m].ec_tou_ub.nrows();
		int num_tiers = end_tier - start_tier + 1;

		if (!gen_only) // added for two meter no load scenarios to use load tier sizing
		{
			//start_tier = 0;
			end_tier = (int)m_month[m].ec_tou_ub_init.ncols() - 1;
			//int num_periods = (int)m_month[m].ec_tou_ub_init.nrows();
			num_tiers = end_tier - start_tier + 1;


			// kWh/kW (kWh/kW daily handled in Setup)
			// 1. find kWh/kW tier
			// 2. set min tier and max tier based on next item in ec_tou matrix
			// 3. resize use and chart based on number of tiers in kWh/kW section
			// 4. assumption is that all periods in same month have same tier breakdown
			// 5. assumption is that tier numbering is correct for the kWh/kW breakdown
			// That is, first tier must be kWh/kW
			if ((m_month[m].ec_tou_units.ncols() > 0 && m_month[m].ec_tou_units.nrows() > 0)
				&& ((m_month[m].ec_tou_units.at(0, 0) == 1) || (m_month[m].ec_tou_units.at(0, 0) == 3)))
			{
				// monthly total energy / monthly peak to determine which kWh/kW tier
				double mon_kWhperkW = -m_month[m].energy_net; // load negative
				if (m_month[m].dc_flat_peak != 0)
					mon_kWhperkW /= m_month[m].dc_flat_peak;
				// find correct start and end tier based on kWhperkW band
				start_tier = 1;
				bool found = false;
				for (size_t i_tier = 0; i_tier < m_month[m].ec_tou_units.ncols(); i_tier++)
				{
					int units = (int)m_month[m].ec_tou_units.at(0, i_tier);
					if ((units == 1) || (units == 3))
					{
						if (found)
						{
							end_tier = (int)i_tier - 1;
							break;
						}
						else if (mon_kWhperkW < m_month[m].ec_tou_ub_init.at(0, i_tier))
						{
							start_tier = (int)i_tier + 1;
							found = true;
						}
					}
				}
				// last tier since no max specified in rate
				if (!found) start_tier = end_tier;
				if (start_tier >= (int)m_month[m].ec_tou_ub_init.ncols())
					start_tier = (int)m_month[m].ec_tou_ub_init.ncols() - 1;
				if (end_tier < start_tier)
					end_tier = start_tier;
				num_tiers = end_tier - start_tier + 1;
				// resize everytime to handle load and energy changes
				// resize sr, br and ub for use in energy charge calculations below
				util::matrix_t<float> br(num_periods, num_tiers);
				util::matrix_t<float> sr(num_periods, num_tiers);
				util::matrix_t<float> ub(num_periods, num_tiers);
				// assign appropriate values.
				for (int period = 0; period < num_periods; period++)
				{
					for (int tier = 0; tier < num_tiers; tier++)
					{
						br.at(period, tier) = m_month[m].ec_tou_br_init.at(period, start_tier + tier);
						sr.at(period, tier) = m_month[m].ec_tou_sr_init.at(period, start_tier + tier);
						ub.at(period, tier) = m_month[m].ec_tou_ub_init.at(period, start_tier + tier);
						// update for correct tier number column headings
						m_month[m].ec_periods_tiers[period][tier] = start_tier + m_ec_periods_tiers_init[period][tier];
					}
				}

				m_month[m].ec_tou_br = br;
				m_month[m].ec_tou_sr = sr;
				m_month[m].ec_tou_ub = ub;
			}
		}
		// reset now resized
		start_tier = 0;
		end_tier = (int)m_month[m].ec_tou_ub.ncols() - 1;

		m_month[m].ec_energy_surplus.resize_fill(num_periods, num_tiers, 0);
		m_month[m].ec_energy_use.resize_fill(num_periods, num_tiers, 0);
		m_month[m].ec_charge.resize_fill(num_periods, num_tiers, 0);

	}

}

void rate_data::setup(bool dc_enabled, bool en_ts_sell_rate, size_t cnt, ssc_number_t* ts_sr, ssc_number_t* ts_br,
	ssc_number_t* ec_weekday, ssc_number_t* ec_weekend, size_t ec_tou_rows, ssc_number_t* ec_tou_in, bool sell_eq_buy,
	ssc_number_t* dc_weekday, ssc_number_t* dc_weekend, size_t dc_tou_rows, ssc_number_t* dc_tou_in, size_t dc_flat_rows, ssc_number_t* dc_flat_in)
{
	size_t nrows, ncols, r, c, m, i, j;
	int period, tier, month;
	//		util::matrix_t<float> dc_schedwkday(12, 24, 1);
	//		util::matrix_t<float> dc_schedwkend(12, 24, 1);

	for (i = 0; i < m_ec_periods_tiers_init.size(); i++)
		m_ec_periods_tiers_init[i].clear();
	m_ec_periods.clear();

	for (i = 0; i < m_dc_tou_periods_tiers.size(); i++)
		m_dc_tou_periods_tiers[i].clear();
	m_dc_tou_periods.clear();

	for (i = 0; i < m_dc_flat_tiers.size(); i++)
		m_dc_flat_tiers[i].clear();

	m_month.clear();
	for (m = 0; m < 12; m++)
	{
		ur_month urm;
		m_month.push_back(urm);
	}

	m_ec_ts_sell_rate.clear();
	m_ec_ts_buy_rate.clear();

	bool ec_enabled = true; // per 2/25/16 meeting

	if (en_ts_sell_rate)
	{
		size_t ts_step_per_hour = cnt / 8760;
		// assign timestep values for utility rate calculations
		size_t idx = 0;
		ssc_number_t sr, br;
		sr = br = 0;
		size_t step_per_hour = m_num_rec_yearly / 8760;
		//time step rates - fill out to number of generation records per year
		// handle cases
		// 1. if no time step rate  s
		// 2. if time step rate  has 8760 and gen has more records
		// 3. if number records same for time step rate  and gen
		idx = 0;
		for (i = 0; i < 8760; i++)
		{
			for (size_t ii = 0; ii < step_per_hour; ii++)
			{
				//						size_t ndx = i*step_per_hour + ii;
				sr = (idx < cnt) ? ts_sr[idx] : 0;
				br = (idx < cnt) ? ts_br[idx] : 0;
				m_ec_ts_sell_rate.push_back(sr);
				m_ec_ts_buy_rate.push_back(br);
				if (ii < ts_step_per_hour) idx++;
			}
		}
		
	}

	// for reporting purposes
	for (i = 0; i < m_num_rec_yearly; i++)
	{
		m_ec_tou_sched.push_back(1);
		m_dc_tou_sched.push_back(1);
	}

	size_t idx = 0;
	size_t steps_per_hour = m_num_rec_yearly / 8760;

	if (ec_enabled)
	{
		// This is error checked in cmod_utilitrate5 (and other calling functions)
		nrows = 12;
		ncols = 24;

		util::matrix_t<double> ec_schedwkday(nrows, ncols);
		ec_schedwkday.assign(ec_weekday, nrows, ncols);
		util::matrix_t<double> ec_schedwkend(nrows, ncols);
		ec_schedwkend.assign(ec_weekend, nrows, ncols);

		// for each row (month) determine periods in the month
		// m_monthly_ec_tou_ub max of period tier matrix of period xtier +1
		// columns are period, tier1 max, tier 2 max, ..., tier n max


		int ec_tod[8760];

		if (!util::translate_schedule(ec_tod, ec_schedwkday, ec_schedwkend, 1, 12))
			throw general_error("Could not translate weekday and weekend schedules for energy rates.");
		for (i = 0; i < 8760; i++)
		{
			for (size_t ii = 0; ii < steps_per_hour; ii++)
			{
				if (idx < m_num_rec_yearly)
					m_ec_tou_sched[idx] = ec_tod[i];
				idx++;
			}
		}

		// 6 columns period, tier, max usage, max usage units, buy, sell
		ncols = 6;
		util::matrix_t<double> ec_tou_mat(ec_tou_rows, ncols);
		ec_tou_mat.assign(ec_tou_in, ec_tou_rows, ncols);

		for (r = 0; r < ec_tou_rows; r++)
		{
			period = (int)ec_tou_mat.at(r, 0);
			if (std::find(m_ec_periods.begin(), m_ec_periods.end(), period) == m_ec_periods.end())
				m_ec_periods.push_back(period);
		}
		// sorted periods smallest to largest
		std::sort(m_ec_periods.begin(), m_ec_periods.end());
		// for each period, get list of tier numbers and then sort and construct
		//m_ec_tou_ub, m_ec_tou_units, m_ec_tou_br, ec_tou_sr vectors of vectors

		for (r = 0; r < m_ec_periods.size(); r++)
		{
			m_ec_periods_tiers_init.push_back(std::vector<int>());
		}

		for (r = 0; r < ec_tou_rows; r++)
		{
			period = (int)ec_tou_mat.at(r, 0);
			tier = (int)ec_tou_mat.at(r, 1);
			std::vector<int>::iterator result = std::find(m_ec_periods.begin(), m_ec_periods.end(), period);
			if (result == m_ec_periods.end())
			{
				std::ostringstream ss;
				ss << "Energy rate Period " << period << " not found.";
				throw exec_error("lib_utility_rate_equations", ss.str());
			}
			int ndx = (int)(result - m_ec_periods.begin());
			m_ec_periods_tiers_init[ndx].push_back(tier);
		}
		// sort tier values for each period
		for (r = 0; r < m_ec_periods_tiers_init.size(); r++)
			std::sort(m_ec_periods_tiers_init[r].begin(), m_ec_periods_tiers_init[r].end());

		// find all periods for each month m through schedules
		for (m = 0; m < m_month.size(); m++)
		{
			// energy charges
			for (c = 0; c < ec_schedwkday.ncols(); c++)
			{
				if (std::find(m_month[m].ec_periods.begin(), m_month[m].ec_periods.end(), ec_schedwkday.at(m, c)) == m_month[m].ec_periods.end())
					m_month[m].ec_periods.push_back((int)ec_schedwkday.at(m, c));

				// rollover periods considered from weekday schedule at 12a [0], 6a [5], 12p [11], and 6p [17]
				if ((m_month[m].ec_rollover_periods.size() < 5) && (c == 0 || c == 5 || c == 11 || c == 17))
				{
					m_month[m].ec_rollover_periods.push_back((int)ec_schedwkday.at(m, c));
				}
			}
			for (c = 0; c < ec_schedwkend.ncols(); c++)
			{
				if (std::find(m_month[m].ec_periods.begin(), m_month[m].ec_periods.end(), ec_schedwkend.at(m, c)) == m_month[m].ec_periods.end())
					m_month[m].ec_periods.push_back((int)ec_schedwkend.at(m, c));
			}
			std::sort(m_month[m].ec_periods.begin(), m_month[m].ec_periods.end());

			// set m_ec_periods_tiers
			for (size_t pt_period = 0; pt_period < m_ec_periods_tiers_init.size(); pt_period++)
				m_month[m].ec_periods_tiers.push_back(std::vector<int>());
			for (size_t pt_period = 0; pt_period < m_ec_periods_tiers_init.size(); pt_period++)
			{
				for (size_t pt_tier = 0; pt_tier < m_ec_periods_tiers_init[pt_period].size(); pt_tier++)
					m_month[m].ec_periods_tiers[pt_period].push_back(m_ec_periods_tiers_init[pt_period][pt_tier]);
			}


		}

		// periods are rows and tiers are columns - note that columns can change based on rows
		// Initialize each month variables that are constant over the simulation
		for (m = 0; m < m_month.size(); m++)
		{
			int num_periods = 0;
			int num_tiers = 0;

			for (i = 0; i < m_month[m].ec_periods.size(); i++)
			{
				// find all periods and check that number of tiers the same for all for the month, if not through error
				std::vector<int>::iterator per_num = std::find(m_ec_periods.begin(), m_ec_periods.end(), m_month[m].ec_periods[i]);
				if (per_num == m_ec_periods.end())
				{
					std::ostringstream ss;
					ss << "Period " << m_month[m].ec_periods[i] << " is in Month " << m << " but is not defined in the energy rate table. Rates for each period in the Weekday and Weekend schedules must be defined in the energy rate table.";
					throw exec_error("lib_utility_rate_equations", ss.str());
				}
				period = (*per_num);
				int ndx = (int)(per_num - m_ec_periods.begin());
				num_tiers = (int)m_ec_periods_tiers_init[ndx].size();
				if (i == 0)
				{
					// redimension ec_ field of ur_month class
					num_periods = (int)m_month[m].ec_periods.size();
					m_month[m].ec_tou_ub.resize_fill(num_periods, num_tiers, (ssc_number_t)1e+38);
					m_month[m].ec_tou_units.resize_fill(num_periods, num_tiers, 0); // kWh
					m_month[m].ec_tou_br.resize_fill(num_periods, num_tiers, 0);
					m_month[m].ec_tou_sr.resize_fill(num_periods, num_tiers, 0);
				}
				else
				{
					if ((int)m_ec_periods_tiers_init[ndx].size() != num_tiers)
					{
						std::ostringstream ss;
						ss << "The number of tiers in the energy rate table, " << m_ec_periods_tiers_init[ndx].size() << ", is incorrect for Month " << m << " and Period " << m_month[m].ec_periods[i] << ". The correct number of tiers for that month and period is " << num_tiers << ".";
						throw exec_error("lib_utility_rate_equations", ss.str());
					}
				}
				for (j = 0; j < m_ec_periods_tiers_init[ndx].size(); j++)
				{
					tier = m_ec_periods_tiers_init[ndx][j];
					// initialize for each period and tier
					bool found = false;
					for (r = 0; (r < ec_tou_rows) && !found; r++)
					{
						if ((period == (int)ec_tou_mat.at(r, 0))
							&& (tier == (int)ec_tou_mat.at(r, 1)))
						{
							m_month[m].ec_tou_ub.at(i, j) = ec_tou_mat.at(r, 2);
							// units kWh, kWh/kW, kWh daily, kWh/kW daily
							m_month[m].ec_tou_units.at(i, j) = (int)ec_tou_mat.at(r, 3);
							if ((m_month[m].ec_tou_units.at(i, j) == 2)
								|| (m_month[m].ec_tou_units.at(i, j) == 3))
							{// kWh daily or kWh/kW daily - adjust max usage by number of days in month (or billing cycle per Eric 12/14/15
								m_month[m].ec_tou_ub.at(i, j) *= util::nday[m];
							}
							m_month[m].ec_tou_br.at(i, j) = ec_tou_mat.at(r, 4);
							// adjust sell rate based on input selections
							ssc_number_t sell = ec_tou_mat.at(r, 5);
							if (sell_eq_buy)
								sell = ec_tou_mat.at(r, 4);
							m_month[m].ec_tou_sr.at(i, j) = sell;
							found = true;
						}
					}

				}
				// copy all sr, br, ub for ec in case units force change
				// copy not same memory location
				m_month[m].ec_tou_ub_init = m_month[m].ec_tou_ub;
				m_month[m].ec_tou_br_init = m_month[m].ec_tou_br;
				m_month[m].ec_tou_sr_init = m_month[m].ec_tou_sr;
			}
		}

	}


	// demand charge initialization
	if (dc_enabled)
	{
		// This is error checked in cmod_utilitrate5 (and other calling functions)
		nrows = 12;
		ncols = 24;
		util::matrix_t<double> dc_schedwkday(nrows, ncols);
		dc_schedwkday.assign(dc_weekday, nrows, ncols);
		util::matrix_t<double> dc_schedwkend(nrows, ncols);
		dc_schedwkend.assign(dc_weekend, nrows, ncols);

		// for each row (month) determine periods in the month
		// m_monthly_dc_tou_ub max of period tier matrix of period xtier +1
		// columns are period, tier1 max, tier 2 max, ..., tier n max


		int dc_tod[8760];

		if (!util::translate_schedule(dc_tod, dc_schedwkday, dc_schedwkend, 1, 12))
			throw general_error("Could not translate weekday and weekend schedules for demand charges");

		idx = 0;
		for (i = 0; i < 8760; i++)
		{
			for (size_t ii = 0; ii < steps_per_hour; ii++)
			{
				if (idx < m_num_rec_yearly)
					m_dc_tou_sched[idx] = dc_tod[i];
				idx++;
			}
		}

		// 4 columns period, tier, max usage, charge
		ncols = 4;
		util::matrix_t<double> dc_tou_mat(dc_tou_rows, ncols);
		dc_tou_mat.assign(dc_tou_in, dc_tou_rows, ncols);

		// find all periods for each month m through schedules
		for (m = 0; m < m_month.size(); m++)
		{
			// demand charges
			for (c = 0; c < dc_schedwkday.ncols(); c++)
			{
				if (std::find(m_month[m].dc_periods.begin(), m_month[m].dc_periods.end(), dc_schedwkday.at(m, c)) == m_month[m].dc_periods.end())
					m_month[m].dc_periods.push_back((int)dc_schedwkday.at(m, c));
			}
			for (c = 0; c < dc_schedwkend.ncols(); c++)
			{
				if (std::find(m_month[m].dc_periods.begin(), m_month[m].dc_periods.end(), dc_schedwkend.at(m, c)) == m_month[m].dc_periods.end())
					m_month[m].dc_periods.push_back((int)dc_schedwkend.at(m, c));
			}
			std::sort(m_month[m].dc_periods.begin(), m_month[m].dc_periods.end());
		}

		for (r = 0; r < dc_tou_rows; r++)
		{
			period = (int)dc_tou_mat.at(r, 0);
			if (std::find(m_dc_tou_periods.begin(), m_dc_tou_periods.end(), period) == m_dc_tou_periods.end())
				m_dc_tou_periods.push_back(period);
		}
		// sorted periods smallest to largest
		std::sort(m_dc_tou_periods.begin(), m_dc_tou_periods.end());
		// for each period, get list of tier numbers and then sort and construct
		//m_dc_tou_ub, m_dc_tou_units, m_dc_tou_br, dc_tou_sr vectors of vectors
		for (r = 0; r < m_dc_tou_periods.size(); r++)
		{
			m_dc_tou_periods_tiers.push_back(std::vector<int>());
		}

		for (r = 0; r < dc_tou_rows; r++)
		{
			period = (int)dc_tou_mat.at(r, 0);
			tier = (int)dc_tou_mat.at(r, 1);
			std::vector<int>::iterator result = std::find(m_dc_tou_periods.begin(), m_dc_tou_periods.end(), period);
			if (result == m_dc_tou_periods.end())
			{
				std::ostringstream ss;
				ss << "Demand charge Period " << period << " not found.";
				throw exec_error("utilityrate5", ss.str());
			}
			int ndx = (int)(result - m_dc_tou_periods.begin());
			m_dc_tou_periods_tiers[ndx].push_back(tier);
		}
		// sort tier values for each period
		for (r = 0; r < m_dc_tou_periods_tiers.size(); r++)
			std::sort(m_dc_tou_periods_tiers[r].begin(), m_dc_tou_periods_tiers[r].end());


		// periods are rows and tiers are columns - note that columns can change based on rows
		// Initialize each month variables that are constant over the simulation

		for (m = 0; m < m_month.size(); m++)
		{
			int num_periods = 0;
			int num_tiers = 0;
			num_periods = (int)m_month[m].dc_periods.size();
			for (i = 0; i < m_month[m].dc_periods.size(); i++)
			{
				// find all periods and check that number of tiers the same for all for the month, if not through error
				std::vector<int>::iterator per_num = std::find(m_dc_tou_periods.begin(), m_dc_tou_periods.end(), m_month[m].dc_periods[i]);
				if (per_num == m_dc_tou_periods.end())
				{
					std::ostringstream ss;
					ss << "Period " << m_month[m].dc_periods[i] << " is in Month " << m << " but is not defined in the demand rate table.  Rates for each period in the Weekday and Weekend schedules must be defined in the demand rate table.";
					throw exec_error("utilityrate5", ss.str());
				}
				period = (*per_num);
				int ndx = (int)(per_num - m_dc_tou_periods.begin());
				// redimension dc_ field of ur_month class
				if ((int)m_dc_tou_periods_tiers[ndx].size() > num_tiers)
					num_tiers = (int)m_dc_tou_periods_tiers[ndx].size();
			}
			m_month[m].dc_tou_ub.resize_fill(num_periods, num_tiers, (ssc_number_t)1e38);
			m_month[m].dc_tou_ch.resize_fill(num_periods, num_tiers, 0); // kWh
			for (i = 0; i < m_month[m].dc_periods.size(); i++)
			{
				// find all periods and check that number of tiers the same for all for the month, if not through error
				std::vector<int>::iterator per_num = std::find(m_dc_tou_periods.begin(), m_dc_tou_periods.end(), m_month[m].dc_periods[i]);
				period = (*per_num);
				int ndx = (int)(per_num - m_dc_tou_periods.begin());
				for (j = 0; j < m_dc_tou_periods_tiers[ndx].size(); j++)
				{
					tier = m_dc_tou_periods_tiers[ndx][j];
					// initialize for each period and tier
					bool found = false;
					for (r = 0; (r < dc_tou_rows) && !found; r++)
					{
						if ((period == (int)dc_tou_mat.at(r, 0))
							&& (tier == (int)dc_tou_mat.at(r, 1)))
						{
							m_month[m].dc_tou_ub.at(i, j) = dc_tou_mat.at(r, 2);
							m_month[m].dc_tou_ch.at(i, j) = dc_tou_mat.at(r, 3);//demand charge;
							found = true;
						}
					}
				}
			}
		}
		// flat demand charge
		// 4 columns month, tier, max usage, charge
		ncols = 4;
		util::matrix_t<double> dc_flat_mat(dc_flat_rows, ncols);
		dc_flat_mat.assign(dc_flat_in, dc_flat_rows, ncols);

		for (r = 0; r < m_month.size(); r++)
		{
			m_dc_flat_tiers.push_back(std::vector<int>());
		}

		for (r = 0; r < dc_flat_rows; r++)
		{
			month = (int)dc_flat_mat.at(r, 0);
			tier = (int)dc_flat_mat.at(r, 1);
			if ((month < 0) || (month >= (int)m_month.size()))
			{
				std::ostringstream ss;
				ss << "Demand for Month " << month << " not found.";
				throw exec_error("utilityrate5", ss.str());
			}
			m_dc_flat_tiers[month].push_back(tier);
		}
		// sort tier values for each period
		for (r = 0; r < m_dc_flat_tiers.size(); r++)
			std::sort(m_dc_flat_tiers[r].begin(), m_dc_flat_tiers[r].end());


		// months are rows and tiers are columns - note that columns can change based on rows
		// Initialize each month variables that are constant over the simulation



		for (m = 0; m < m_month.size(); m++)
		{
			m_month[m].dc_flat_ub.clear();
			m_month[m].dc_flat_ch.clear();
			for (j = 0; j < m_dc_flat_tiers[m].size(); j++)
			{
				tier = m_dc_flat_tiers[m][j];
				// initialize for each period and tier
				bool found = false;
				for (r = 0; (r < dc_flat_rows) && !found; r++)
				{
					if ((m == dc_flat_mat.at(r, 0))
						&& (tier == (int)dc_flat_mat.at(r, 1)))
					{
						m_month[m].dc_flat_ub.push_back(dc_flat_mat.at(r, 2));
						m_month[m].dc_flat_ch.push_back(dc_flat_mat.at(r, 3));//rate_esc;
						found = true;
					}
				}

			}
		}

	}

}