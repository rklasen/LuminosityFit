#ifndef PNDLMDMODELFACTORY_H_
#define PNDLMDMODELFACTORY_H_

#include "core/Model1D.h"
#include "core/Model2D.h"

#include "LumiFitStructs.h"
#include "data/PndLmdAcceptance.h"
#include "data/PndLmdHistogramData.h"
#include "data/PndLmdMapData.h"
#include "model/PndLmdSmearingModel2D.h"

#include "boost/property_tree/ptree_fwd.hpp"

class PndLmdAngularData;

/**
 * Class for creating Models. User is supposed to only use this factory to create
 * his models.
 */
class PndLmdModelFactory {
private:
  PndLmdAcceptance acceptance;
  std::vector<PndLmdHistogramData> resolutions;
  PndLmdMapData resolution_map_data;

  std::shared_ptr<PndLmdSmearingModel2D> generate2DSmearingModel(
      const LumiFit::LmdDimension &dimx,
      const LumiFit::LmdDimension &dimy) const;

  std::shared_ptr<PndLmdSmearingModel2D> generate2DSmearingModel(
      const PndLmdHistogramData &data, const LumiFit::LmdDimension &dimx,
      const LumiFit::LmdDimension &dimy) const;

  /**
   * 1D Model generator method
   * @param model_opt_ptree are the options which model will be built and returned
   */
  std::shared_ptr<Model1D> generate1DModel(
      const boost::property_tree::ptree& model_opt_ptree,
      const PndLmdAngularData& data) const;

  /**
   * 2D Model generator method
   * @param model_opt_ptree are the options which model will be built and returned
   */
  std::shared_ptr<Model2D> generate2DModel(
      const boost::property_tree::ptree& model_opt_ptree,
      const PndLmdAngularData& data) const;
public:
  PndLmdModelFactory();
  ~PndLmdModelFactory();

  double getMomentumTransferFromTheta(double plab, double theta) const;

  const PndLmdAcceptance& getAcceptance() const;
  const PndLmdMapData& getResolutionMapData() const;
  void setAcceptance(const PndLmdAcceptance& acceptance_);
  void setResolutionMapData(const PndLmdMapData& res_map_);
  void setResolutions(const std::vector<PndLmdHistogramData>& resolutions_);

  std::shared_ptr<Model1D> generate1DVertexModel(
      const boost::property_tree::ptree& model_opt_ptree) const;

  std::shared_ptr<Model> generateModel(
      const boost::property_tree::ptree& model_opt_ptree,
      const PndLmdAngularData& data) const;
};

#endif /* PNDLMDMODELFACTORY_H_ */
