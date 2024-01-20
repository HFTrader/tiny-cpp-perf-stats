#include "Snapshot.h"
#include "Counter.h"
#include "Product.h"
#include "IndexedMap.h"
#define ARMA_DONT_PRINT_ERRORS
#include <armadillo>
#include <set>
#include <boost/math/distributions.hpp>

Snapshot::Snapshot(const std::vector<std::string> &pmc) {
    if (!counters.init(pmc)) {
        std::cerr << "Unable to initialize performance counters group" << '\n';
    }
    debug = 0;
}

Snapshot::Snapshot() {
    std::vector<std::string> counter_names{"cycles", "instructions", "cache-misses",
                                           "branch-misses"};
    if (!counters.init(counter_names)) {
        std::cerr << "Unable to initialize performance counters group" << '\n';
    }
    debug = 0;
}

Snapshot::~Snapshot() {
}

void Snapshot::start() {
    counters.start();
}

Snapshot::Sample Snapshot::stop(const std::string &event_name, uint64_t numitems,
                                uint64_t numiterations) {
    auto [it, inserted] = events.insert({event_name, events.size()});
    int eventid = it->second;
    counters.stop();
    Sample samp;
    if (numiterations > 0) {
        samp.push_back({"Constant", (double)1, 0, false});
        samp.push_back({"N", (double)numitems, 1, false});
        samp.push_back({"N2", double(numitems) * double(numitems), 1, false});
        samp.push_back({"log(N)", (double)log(numitems) / log(10), 1, false});
        for (size_t j = 0; j < counters.size(); ++j) {
            double value = double(counters[j]) / numiterations;
            samp.push_back({counters.name(j), value, int(2 + j), false});
        }
        samples[event_name].push_back(samp);
    }
    return samp;
}

// I should really spawn this into a LinearModel class
struct RegResults {
    bool ok;
    arma::mat C;
    arma::colvec b;
    arma::colvec sol;
    arma::colvec res;
    arma::colvec serr;
    arma::colvec tval;
    arma::colvec pval;

    double fval;
    double fpval;
    double rsq;
    double rsqadj;
    double loglik;
    double aic;
    double bic;

    bool solve();
};

static bool calcModel(const std::vector<uint32_t> &model, const arma::mat &C,
                      const arma::vec &b, RegResults &reg) {
    uint32_t numcols = model.size();
    uint32_t numrows = C.n_rows;
    reg.b = b;
    reg.C.resize(numrows, numcols);
    for (uint32_t j = 0; j < numcols; ++j) {
        reg.C.col(j) = C.col(model[j]);
    }
    return reg.solve();
}

static arma::colvec cdf(const arma::colvec &x, uint32_t ndof) {
    unsigned nrows = x.n_rows;
    arma::colvec y(nrows);
    boost::math::students_t st(ndof);
    for (unsigned j = 0; j < nrows; ++j) {
        y(j) = cdf(st, x(j));
    }
    return y;
}

bool RegResults::solve() {
    // Dimensionality of the problem
    uint32_t nobs = C.n_rows;
    uint32_t ncoef = C.n_cols;

    // Initialize all metrics to NAN
    rsq = rsqadj = fval = fpval = loglik = aic = bic =
        std::numeric_limits<double>::quiet_NaN();

    // Check dimensions
    if (nobs <= ncoef) return false;

    // Solve system with LSQ
    ok = arma::solve(sol, C, b);
    if (not ok) return false;

    try {
        // Residuals
        res = b - C * sol;

        // Degrees of freedom
        uint32_t ndof = nobs - ncoef;

        // Variance of residuals
        double s2 = arma::dot(res, res) / ndof;

        // Standard errors
        serr = arma::sqrt(s2 * arma::diagvec(arma::pinv(C.t() * C)));

        // t-values and respective p-values
        tval = sol / serr;
        pval = (1 - cdf(arma::abs(tval), ndof)) * 2;

        // R-squared measures
        rsq = 1 - arma::dot(res, res) / arma::dot(b, b);
        rsqadj = 1 - (1 - rsq) * (double(nobs - 1) / (nobs - ncoef));

        // Compute F-value and respective probability for model selection
        fval = (rsq / (ncoef - 1)) / ((1 - rsq) / ndof);
        boost::math::fisher_f ff(ncoef - 1, nobs - ncoef);
        fpval = 1 - cdf(ff, fval);

        // log likelihood for model selection with Akaike information coefficients
        loglik = -(nobs * 0.5) * (1 + log(2 * M_PI)) -
                 (nobs / 2.) * log(arma::dot(res, res) / nobs);
        aic = -(2. * loglik) / nobs + double(2 * ncoef) / nobs;
        bic = -(2. * loglik) / nobs + double(ncoef * log(nobs)) / nobs;
    } catch (...) {
        // bad luck
        return false;
    }
    return true;
}

void Snapshot::summary(const std::string &header, FILE *f) {
    struct Variable {
        int id = -1;
        int group;
        std::string name;
    };
    struct Group {
        int id = -1;
        std::vector<int> vars;
    };
    // cycle through events map assigning groups and counting total samples to build
    // the stats matrix
    std::string dependent_name = "cycles";
    std::map<std::string, Variable> varmap;
    using GroupKey = std::pair<std::string, int>;
    hb::IndexedMap<GroupKey, Group> groups;
    std::vector<Variable> vars;
    size_t numsamples = 0;
    for (auto &ism : samples) {
        numsamples += ism.second.size();
        std::string event_name = ism.first;
        for (Sample &sample : ism.second) {
            for (Metric &metric : sample) {
                // Skip the dependent variable
                if (metric.name == dependent_name) {
                    continue;
                }

                // Get or create a group for this key
                Group &group(groups[{event_name, metric.group}]);
                if (group.id < 0) {
                    group.id = groups.size() - 1;
                }

                // Recreate the key for this metric - global metric if necessary
                std::string key = (metric.global ? "" : event_name + ":") + metric.name;
                Variable &var(varmap[key]);
                if (var.id < 0) {
                    var.id = varmap.size() - 1;
                    var.name = key;
                    vars.push_back({var.id, group.id, key});
                    group.vars.push_back(var.id);
                }
                var.group = group.id;
            }
        }
    }

    // Build the stats matrix
    size_t numvars = vars.size();
    size_t numgroups = groups.size();
    arma::mat C(numsamples, numvars);
    arma::vec b(numsamples);

    // Count variables
    std::vector<uint32_t> group_counts(numgroups);
    for (uint32_t j = 0; j < numgroups; ++j) group_counts[j] = 0;
    for (const Group &group : groups) {
        group_counts[group.id] = group.vars.size();
    }

    // cycle through all samples, filling up the matrix
    size_t row = 0;
    for (const auto &ism : samples) {
        std::string event_name = ism.first;
        if (debug > 0) {
            fprintf(f, "Sample Items Cycles Cache Instr Branch TLB\n");
        }
        for (const Sample &sample : ism.second) {
            for (const Metric &metric : sample) {
                if (metric.name == dependent_name) {
                    b(row) = metric.value;
                } else {
                    std::string key =
                        (metric.global ? "" : event_name + ":") + metric.name;
                    Variable &var(varmap[key]);

                    if (var.id >= 0) {
                        C(row, var.id) = metric.value;
                    } else {
                        std::cerr << "This should never happen!"
                                  << "\n";
                    }
                }
            }
            row++;
        }
    }

    // Now generate all possible combinations of models
    RegResults bestreg;
    bool found = false;
    std::vector<uint32_t> bestmodel;

    Product<uint32_t> indices(group_counts);
    while (indices.next()) {
        std::cout << "New combination: " << '\n';
        std::vector<uint32_t> model(numgroups);
        for (int ng = 0; ng < numgroups; ++ng) {
            int index = indices[ng];
            int var_index = groups[ng].vars[index];
            model[ng] = var_index;
            std::cout << "    " << vars[var_index].name << '\n';
        }
        RegResults reg;
        if (calcModel(model, C, b, reg)) {
            // if (reg.pval.max() > 0.05) continue;

            if ((not found) or (reg.aic < bestreg.aic)) {
                bestreg = reg;
                bestmodel = model;
                found = true;
            }
        }
    }

    if (not found) {
        fprintf(f, "    Model did not converge or not enough points\n");
    } else {
        fprintf(f,
                "\n========== Best Model\n%s, "
                " Rsq:%5.2f F:%f LL:%f aic:%f bic:%f \n",
                header.c_str(), bestreg.rsq, bestreg.fpval, bestreg.loglik, bestreg.aic,
                bestreg.bic);

        for (unsigned j = 0; j < bestmodel.size(); ++j) {
            fprintf(f, "   Term: %-12s  p:%7.5f coef:%g\n",
                    vars[bestmodel[j]].name.c_str(), bestreg.pval(j), bestreg.sol(j));
        }
        if (debug > 0) {
            for (unsigned j = 0; j < bestmodel.size(); ++j) {
                fprintf(f, "%s ", vars[bestmodel[j]].name.c_str());
            }
            fprintf(f, "\n");
        }
    }
}
